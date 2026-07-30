/**
 *
 * @file postgres_snapshot_store.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the PostgreSQL Vix Realtime snapshot store.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/postgres_snapshot_store.hpp>

#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/json/json.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/types.hpp>

#if defined(VIX_REALTIME_ENABLE_POSTGRES) || \
    defined(VIX_REALTIME_HAS_POSTGRES) ||    \
    defined(VIX_ENABLE_POSTGRES) ||          \
    defined(VIX_DB_USE_POSTGRES)

#define VIX_REALTIME_POSTGRES_COMPILED 1

#include <libpq-fe.h>

#else

#define VIX_REALTIME_POSTGRES_COMPILED 0

#endif

namespace vix::realtime
{
  namespace
  {
    /**
     * @brief Return whether text is a safe unquoted SQL identifier.
     */
    [[nodiscard]] bool valid_identifier(
        std::string_view value) noexcept
    {
      if (value.empty() ||
          value.size() > 63)
      {
        return false;
      }

      const unsigned char first =
          static_cast<unsigned char>(
              value.front());

      if (!(std::isalpha(first) != 0 ||
            first == '_'))
      {
        return false;
      }

      for (const unsigned char character : value)
      {
        if (!(std::isalnum(character) != 0 ||
              character == '_'))
        {
          return false;
        }
      }

      return true;
    }

    /**
     * @brief Quote a previously validated PostgreSQL identifier.
     */
    [[nodiscard]] std::string quote_identifier(
        std::string_view value)
    {
      return "\"" +
             std::string{value} +
             "\"";
    }

    /**
     * @brief Convert a system timestamp to epoch microseconds.
     */
    [[nodiscard]] std::int64_t timestamp_to_micros(
        Timestamp value)
    {
      const auto microseconds =
          std::chrono::duration_cast<
              std::chrono::microseconds>(
              value.time_since_epoch())
              .count();

      if constexpr (
          sizeof(decltype(microseconds)) >
          sizeof(std::int64_t))
      {
        if (microseconds >
                std::numeric_limits<std::int64_t>::max() ||
            microseconds <
                std::numeric_limits<std::int64_t>::min())
        {
          throw Error{
              ErrorCode::SnapshotStoreFailure,
              "room snapshot timestamp exceeds PostgreSQL storage range"};
        }
      }

      return static_cast<std::int64_t>(
          microseconds);
    }

    /**
     * @brief Convert epoch microseconds to a system timestamp.
     */
    [[nodiscard]] Timestamp timestamp_from_micros(
        std::int64_t value)
    {
      return Timestamp{
          std::chrono::duration_cast<
              SystemClock::duration>(
              std::chrono::microseconds{
                  value})};
    }

    /**
     * @brief Parse one signed 64-bit integer.
     */
    [[nodiscard]] std::int64_t parse_i64(
        std::string_view value,
        std::string_view field)
    {
      std::int64_t result = 0;

      const auto conversion =
          std::from_chars(
              value.data(),
              value.data() + value.size(),
              result);

      if (conversion.ec != std::errc{} ||
          conversion.ptr !=
              value.data() + value.size())
      {
        throw Error{
            ErrorCode::CorruptedState,
            std::string{
                "invalid PostgreSQL snapshot integer field: "} +
                std::string{field}};
      }

      return result;
    }

    /**
     * @brief Parse one unsigned 64-bit integer.
     */
    [[nodiscard]] std::uint64_t parse_u64(
        std::string_view value,
        std::string_view field)
    {
      std::uint64_t result = 0;

      const auto conversion =
          std::from_chars(
              value.data(),
              value.data() + value.size(),
              result);

      if (conversion.ec != std::errc{} ||
          conversion.ptr !=
              value.data() + value.size())
      {
        throw Error{
            ErrorCode::CorruptedState,
            std::string{
                "invalid PostgreSQL snapshot unsigned field: "} +
                std::string{field}};
      }

      return result;
    }

    /**
     * @brief Parse one snapshot schema version.
     */
    [[nodiscard]] SchemaVersion parse_schema_version(
        std::string_view value)
    {
      const std::uint64_t parsed =
          parse_u64(
              value,
              "schema_version");

      if (parsed == 0 ||
          parsed >
              std::numeric_limits<SchemaVersion>::max())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "PostgreSQL snapshot contains an invalid schema version"};
      }

      return static_cast<SchemaVersion>(
          parsed);
    }

    /**
     * @brief Convert a JSON value to a Vix JSON token.
     */
    [[nodiscard]] vix::json::token json_to_token(
        const vix::json::Json &value);

    /**
     * @brief Convert a JSON object to a Vix key-value object.
     */
    [[nodiscard]] JsonObject json_to_object(
        const vix::json::Json &value)
    {
      if (!value.is_object())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "PostgreSQL snapshot JSON value must be an object"};
      }

      JsonObject result;
      result.reserve_pairs(
          value.size());

      for (auto iterator = value.begin();
           iterator != value.end();
           ++iterator)
      {
        result.set(
            iterator.key(),
            json_to_token(
                iterator.value()));
      }

      return result;
    }

    vix::json::token json_to_token(
        const vix::json::Json &value)
    {
      if (value.is_null())
      {
        return {};
      }

      if (value.is_boolean())
      {
        return vix::json::token{
            value.get<bool>()};
      }

      if (value.is_number_integer())
      {
        return vix::json::token{
            value.get<std::int64_t>()};
      }

      if (value.is_number_unsigned())
      {
        const std::uint64_t number =
            value.get<std::uint64_t>();

        if (number >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()))
        {
          throw Error{
              ErrorCode::CorruptedState,
              "PostgreSQL snapshot JSON integer exceeds signed 64-bit range"};
        }

        return vix::json::token{
            static_cast<std::int64_t>(
                number)};
      }

      if (value.is_number_float())
      {
        return vix::json::token{
            value.get<double>()};
      }

      if (value.is_string())
      {
        return vix::json::token{
            value.get<std::string>()};
      }

      if (value.is_array())
      {
        vix::json::array_t array;

        array.reserve(
            value.size());

        for (const auto &element : value)
        {
          array.push_back(
              json_to_token(element));
        }

        return vix::json::token{
            std::move(array)};
      }

      if (value.is_object())
      {
        return vix::json::token{
            json_to_object(value)};
      }

      throw Error{
          ErrorCode::CorruptedState,
          "PostgreSQL snapshot contains an unsupported JSON value"};
    }

    /**
     * @brief Parse one serialized Vix JSON object.
     */
    [[nodiscard]] JsonObject parse_json_object(
        std::string_view value,
        std::string_view field)
    {
      try
      {
        const vix::json::Json parsed =
            vix::json::Json::parse(value);

        return json_to_object(parsed);
      }
      catch (const Error &)
      {
        throw;
      }
      catch (const std::exception &error)
      {
        throw Error{
            ErrorCode::CorruptedState,
            std::string{
                "failed to parse PostgreSQL snapshot "} +
                std::string{field} +
                ": " +
                error.what()};
      }
      catch (...)
      {
        throw Error{
            ErrorCode::CorruptedState,
            std::string{
                "failed to parse PostgreSQL snapshot "} +
                std::string{field}};
      }
    }

    /**
     * @brief Serialize a Vix JSON object for PostgreSQL JSONB.
     */
    [[nodiscard]] std::string serialize_json_object(
        const JsonObject &value)
    {
      try
      {
        return vix::json::to_json(value)
            .dump();
      }
      catch (const std::exception &error)
      {
        throw Error{
            ErrorCode::SnapshotStoreFailure,
            std::string{
                "failed to serialize room snapshot JSON: "} +
                error.what()};
      }
      catch (...)
      {
        throw Error{
            ErrorCode::SnapshotStoreFailure,
            "failed to serialize room snapshot JSON"};
      }
    }

    /**
     * @brief Convert a platform size to a PostgreSQL BIGINT parameter.
     */
    [[nodiscard]] std::string size_parameter(
        std::size_t value)
    {
      const std::uint64_t converted =
          static_cast<std::uint64_t>(
              value);

      const std::uint64_t maximum =
          static_cast<std::uint64_t>(
              std::numeric_limits<std::int64_t>::max());

      return std::to_string(
          converted > maximum
              ? maximum
              : converted);
    }

  } // namespace

  void PostgresSnapshotStoreOptions::validate() const
  {
    if (connectionString.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "PostgreSQL snapshot store requires a connection string"};
    }

    if (!valid_identifier(schema))
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "PostgreSQL snapshot store schema name is invalid"};
    }

    if (!valid_identifier(table))
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "PostgreSQL snapshot store table name is invalid"};
    }
  }

#if VIX_REALTIME_POSTGRES_COMPILED

  namespace
  {
    /**
     * @brief Automatically clear a libpq result.
     */
    struct ResultDeleter
    {
      void operator()(PGresult *result) const noexcept
      {
        if (result != nullptr)
        {
          PQclear(result);
        }
      }
    };

    using ResultPtr =
        std::unique_ptr<
            PGresult,
            ResultDeleter>;

    /**
     * @brief Return the most useful libpq error message.
     */
    [[nodiscard]] std::string postgres_error(
        PGconn *connection,
        PGresult *result,
        std::string_view context)
    {
      std::string detail;

      if (result != nullptr)
      {
        const char *message =
            PQresultErrorMessage(result);

        if (message != nullptr)
        {
          detail = message;
        }
      }

      if (detail.empty() &&
          connection != nullptr)
      {
        const char *message =
            PQerrorMessage(connection);

        if (message != nullptr)
        {
          detail = message;
        }
      }

      while (!detail.empty() &&
             (detail.back() == '\n' ||
              detail.back() == '\r'))
      {
        detail.pop_back();
      }

      if (detail.empty())
      {
        return std::string{context};
      }

      return std::string{context} +
             ": " +
             detail;
    }

    /**
     * @brief Execute one SQL command without parameters.
     */
    [[nodiscard]] ResultPtr execute_command(
        PGconn *connection,
        std::string_view sql,
        std::string_view context)
    {
      const std::string query{sql};

      ResultPtr result{
          PQexec(
              connection,
              query.c_str())};

      if (!result ||
          PQresultStatus(result.get()) !=
              PGRES_COMMAND_OK)
      {
        throw Error{
            ErrorCode::SnapshotStoreFailure,
            postgres_error(
                connection,
                result.get(),
                context)};
      }

      return result;
    }

    /**
     * @brief Execute one SQL statement with text parameters.
     */
    [[nodiscard]] ResultPtr execute_parameters(
        PGconn *connection,
        std::string_view sql,
        const std::vector<
            std::optional<std::string>> &parameters,
        ExecStatusType expectedStatus,
        std::string_view context)
    {
      if (parameters.size() >
          static_cast<std::size_t>(
              std::numeric_limits<int>::max()))
      {
        throw Error{
            ErrorCode::SnapshotStoreFailure,
            "PostgreSQL parameter count exceeds libpq limits"};
      }

      const std::string query{sql};

      std::vector<const char *> values;
      values.reserve(
          parameters.size());

      for (const auto &parameter : parameters)
      {
        values.push_back(
            parameter
                ? parameter->c_str()
                : nullptr);
      }

      ResultPtr result{
          PQexecParams(
              connection,
              query.c_str(),
              static_cast<int>(
                  parameters.size()),
              nullptr,
              values.data(),
              nullptr,
              nullptr,
              0)};

      if (!result ||
          PQresultStatus(result.get()) !=
              expectedStatus)
      {
        throw Error{
            ErrorCode::SnapshotStoreFailure,
            postgres_error(
                connection,
                result.get(),
                context)};
      }

      return result;
    }

    /**
     * @brief PostgreSQL transaction with automatic rollback.
     */
    class Transaction
    {
    public:
      explicit Transaction(PGconn *connection)
          : connection_(connection)
      {
        static_cast<void>(
            execute_command(
                connection_,
                "BEGIN",
                "failed to begin PostgreSQL snapshot transaction"));
      }

      ~Transaction()
      {
        if (!committed_)
        {
          PGresult *result =
              PQexec(
                  connection_,
                  "ROLLBACK");

          if (result != nullptr)
          {
            PQclear(result);
          }
        }
      }

      Transaction(
          const Transaction &) = delete;

      Transaction &operator=(
          const Transaction &) = delete;

      void commit()
      {
        static_cast<void>(
            execute_command(
                connection_,
                "COMMIT",
                "failed to commit PostgreSQL snapshot transaction"));

        committed_ = true;
      }

    private:
      PGconn *connection_{nullptr};
      bool committed_{false};
    };

    /**
     * @brief Return one non-null PostgreSQL text field.
     */
    [[nodiscard]] std::string_view result_value(
        PGresult *result,
        int row,
        int column,
        std::string_view field)
    {
      if (PQgetisnull(
              result,
              row,
              column) != 0)
      {
        throw Error{
            ErrorCode::CorruptedState,
            std::string{
                "PostgreSQL snapshot field is null: "} +
                std::string{field}};
      }

      return {
          PQgetvalue(
              result,
              row,
              column),
          static_cast<std::size_t>(
              PQgetlength(
                  result,
                  row,
                  column))};
    }

    /**
     * @brief Return one optional PostgreSQL text field.
     */
    [[nodiscard]] std::optional<std::string>
    optional_result_value(
        PGresult *result,
        int row,
        int column)
    {
      if (PQgetisnull(
              result,
              row,
              column) != 0)
      {
        return std::nullopt;
      }

      return std::string{
          PQgetvalue(
              result,
              row,
              column),
          static_cast<std::size_t>(
              PQgetlength(
                  result,
                  row,
                  column))};
    }

    /**
     * @brief Return the affected row count of a PostgreSQL command.
     */
    [[nodiscard]] std::size_t affected_rows(
        PGresult *result)
    {
      const char *value =
          PQcmdTuples(result);

      if (value == nullptr ||
          *value == '\0')
      {
        return 0;
      }

      const std::uint64_t parsed =
          parse_u64(
              value,
              "affected_snapshot_count");

      if (parsed >
          std::numeric_limits<std::size_t>::max())
      {
        throw Error{
            ErrorCode::SnapshotStoreFailure,
            "PostgreSQL affected snapshot count exceeds platform limits"};
      }

      return static_cast<std::size_t>(
          parsed);
    }

  } // namespace

  struct PostgresSnapshotStore::Impl
  {
    explicit Impl(
        PostgresSnapshotStoreOptions configuredOptions)
        : options(std::move(configuredOptions)),
          qualifiedTable(
              quote_identifier(options.schema) +
              "." +
              quote_identifier(options.table))
    {
    }

    ~Impl()
    {
      if (connection != nullptr)
      {
        PQfinish(connection);
      }
    }

    /**
     * @brief Establish or restore the libpq connection.
     */
    void ensure_connected_locked() const
    {
      if (connection == nullptr)
      {
        connection =
            PQconnectdb(
                options.connectionString.c_str());

        if (connection == nullptr ||
            PQstatus(connection) !=
                CONNECTION_OK)
        {
          const std::string message =
              postgres_error(
                  connection,
                  nullptr,
                  "failed to connect PostgreSQL snapshot store");

          if (connection != nullptr)
          {
            PQfinish(connection);
            connection = nullptr;
          }

          throw Error{
              ErrorCode::SnapshotStoreFailure,
              message};
        }

        if (PQsetClientEncoding(
                connection,
                "UTF8") != 0)
        {
          throw Error{
              ErrorCode::SnapshotStoreFailure,
              postgres_error(
                  connection,
                  nullptr,
                  "failed to configure PostgreSQL UTF-8 encoding")};
        }

        return;
      }

      if (PQstatus(connection) ==
          CONNECTION_OK)
      {
        return;
      }

      if (!options.reconnect)
      {
        throw Error{
            ErrorCode::SnapshotStoreFailure,
            postgres_error(
                connection,
                nullptr,
                "PostgreSQL snapshot store connection is unavailable")};
      }

      PQreset(connection);

      if (PQstatus(connection) !=
          CONNECTION_OK)
      {
        throw Error{
            ErrorCode::SnapshotStoreFailure,
            postgres_error(
                connection,
                nullptr,
                "failed to reconnect PostgreSQL snapshot store")};
      }

      if (PQsetClientEncoding(
              connection,
              "UTF8") != 0)
      {
        throw Error{
            ErrorCode::SnapshotStoreFailure,
            postgres_error(
                connection,
                nullptr,
                "failed to restore PostgreSQL UTF-8 encoding")};
      }
    }

    /**
     * @brief Initialize the configured schema and snapshot table.
     */
    void ensure_ready_locked() const
    {
      ensure_connected_locked();

      if (initialized)
      {
        return;
      }

      if (options.createSchemaIfMissing)
      {
        static_cast<void>(
            execute_command(
                connection,
                "CREATE SCHEMA IF NOT EXISTS " +
                    quote_identifier(
                        options.schema),
                "failed to create PostgreSQL snapshot schema"));
      }

      if (options.createTableIfMissing)
      {
        const std::string sql =
            "CREATE TABLE IF NOT EXISTS " +
            qualifiedTable +
            " ("
            "room_id TEXT NOT NULL,"
            "room_version BIGINT NOT NULL "
            "CHECK (room_version >= 0),"
            "last_event_id BIGINT NOT NULL "
            "CHECK (last_event_id >= 0),"
            "state JSONB NOT NULL,"
            "schema_version INTEGER NOT NULL "
            "CHECK (schema_version > 0),"
            "created_at_micros BIGINT NOT NULL,"
            "checksum TEXT NULL,"
            "metadata JSONB NOT NULL,"
            "PRIMARY KEY (room_id, room_version)"
            ")";

        static_cast<void>(
            execute_command(
                connection,
                sql,
                "failed to create PostgreSQL snapshot table"));
      }

      initialized = true;
    }

    /**
     * @brief Acquire the room-specific transaction advisory lock.
     */
    void lock_room_locked(
        const RoomId &roomId) const
    {
      static_cast<void>(
          execute_parameters(
              connection,
              "SELECT pg_advisory_xact_lock("
              "hashtextextended($1, 1))",
              {
                  roomId.value(),
              },
              PGRES_TUPLES_OK,
              "failed to lock PostgreSQL room snapshot stream"));
    }

    /**
     * @brief Decode one PostgreSQL snapshot row.
     */
    [[nodiscard]] RoomSnapshot decode_snapshot_locked(
        const RoomId &roomId,
        PGresult *result,
        int row) const
    {
      const RoomVersion roomVersion{
          parse_i64(
              result_value(
                  result,
                  row,
                  0,
                  "room_version"),
              "room_version")};

      const EventId lastEventId{
          parse_i64(
              result_value(
                  result,
                  row,
                  1,
                  "last_event_id"),
              "last_event_id")};

      JsonObject state =
          parse_json_object(
              result_value(
                  result,
                  row,
                  2,
                  "state"),
              "state");

      const SchemaVersion schemaVersion =
          parse_schema_version(
              result_value(
                  result,
                  row,
                  3,
                  "schema_version"));

      const Timestamp createdAt =
          timestamp_from_micros(
              parse_i64(
                  result_value(
                      result,
                      row,
                      4,
                      "created_at_micros"),
                  "created_at_micros"));

      const std::optional<std::string> checksum =
          optional_result_value(
              result,
              row,
              5);

      JsonObject metadata =
          parse_json_object(
              result_value(
                  result,
                  row,
                  6,
                  "metadata"),
              "metadata");

      RoomSnapshot snapshot{
          roomId,
          roomVersion,
          lastEventId,
          std::move(state),
          schemaVersion};

      snapshot
          .set_created_at(createdAt)
          .set_metadata(std::move(metadata));

      if (checksum)
      {
        snapshot.set_checksum(
            *checksum);
      }

      snapshot.validate();
      return snapshot;
    }

    /** @brief PostgreSQL snapshot store configuration. */
    PostgresSnapshotStoreOptions options{};

    /** @brief Fully qualified and quoted snapshot table name. */
    std::string qualifiedTable{};

    /** @brief Serializes access to the single libpq connection. */
    mutable std::mutex mutex{};

    /** @brief Owned libpq connection. */
    mutable PGconn *connection{nullptr};

    /** @brief Whether optional schema initialization completed. */
    mutable bool initialized{false};
  };

#else

  struct PostgresSnapshotStore::Impl
  {
    explicit Impl(
        PostgresSnapshotStoreOptions configuredOptions)
        : options(std::move(configuredOptions))
    {
    }

    PostgresSnapshotStoreOptions options{};
  };

#endif

  PostgresSnapshotStore::PostgresSnapshotStore(
      std::string connectionString)
      : PostgresSnapshotStore(
            PostgresSnapshotStoreOptions{
                std::move(connectionString)})
  {
  }

  PostgresSnapshotStore::PostgresSnapshotStore(
      PostgresSnapshotStoreOptions options)
      : impl_(
            std::make_unique<Impl>(
                std::move(options)))
  {
    impl_->options.validate();

#if VIX_REALTIME_POSTGRES_COMPILED

    std::lock_guard<std::mutex> lock{
        impl_->mutex};

    impl_->ensure_ready_locked();

#else

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  PostgresSnapshotStore::~PostgresSnapshotStore() = default;

  RoomSnapshot PostgresSnapshotStore::save(
      RoomSnapshot snapshot)
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    snapshot.validate();

    const RoomId roomId =
        snapshot.room_id();

    std::lock_guard<std::mutex> lock{
        impl_->mutex};

    impl_->ensure_ready_locked();

    Transaction transaction{
        impl_->connection};

    impl_->lock_room_locked(roomId);

    ResultPtr existing =
        execute_parameters(
            impl_->connection,
            "SELECT last_event_id "
            "FROM " +
                impl_->qualifiedTable +
                " WHERE room_id = $1 "
                "AND room_version = $2",
            {
                roomId.value(),
                std::to_string(
                    snapshot.room_version().value()),
            },
            PGRES_TUPLES_OK,
            "failed to inspect existing PostgreSQL room snapshot");

    if (PQntuples(existing.get()) > 1)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "PostgreSQL snapshot identity is not unique"};
    }

    if (PQntuples(existing.get()) == 1)
    {
      const EventId storedEventId{
          parse_i64(
              result_value(
                  existing.get(),
                  0,
                  0,
                  "last_event_id"),
              "last_event_id")};

      if (storedEventId !=
          snapshot.last_event_id())
      {
        throw Error{
            ErrorCode::SnapshotStoreFailure,
            "cannot replace a snapshot version with another event position"};
      }
    }

    std::optional<std::string> checksum;

    if (snapshot.checksum())
    {
      checksum =
          *snapshot.checksum();
    }

    static_cast<void>(
        execute_parameters(
            impl_->connection,
            "INSERT INTO " +
                impl_->qualifiedTable +
                " ("
                "room_id,"
                "room_version,"
                "last_event_id,"
                "state,"
                "schema_version,"
                "created_at_micros,"
                "checksum,"
                "metadata"
                ") VALUES ("
                "$1,$2,$3,$4::jsonb,$5,$6,$7,$8::jsonb"
                ") "
                "ON CONFLICT (room_id, room_version) "
                "DO UPDATE SET "
                "last_event_id = EXCLUDED.last_event_id,"
                "state = EXCLUDED.state,"
                "schema_version = EXCLUDED.schema_version,"
                "created_at_micros = EXCLUDED.created_at_micros,"
                "checksum = EXCLUDED.checksum,"
                "metadata = EXCLUDED.metadata",
            {
                roomId.value(),
                std::to_string(
                    snapshot.room_version().value()),
                std::to_string(
                    snapshot.last_event_id().value()),
                serialize_json_object(
                    snapshot.state()),
                std::to_string(
                    snapshot.schema_version()),
                std::to_string(
                    timestamp_to_micros(
                        snapshot.created_at())),
                std::move(checksum),
                serialize_json_object(
                    snapshot.metadata()),
            },
            PGRES_COMMAND_OK,
            "failed to save PostgreSQL room snapshot"));

    transaction.commit();
    return snapshot;

#else

    static_cast<void>(snapshot);

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  std::optional<RoomSnapshot>
  PostgresSnapshotStore::load_latest(
      const RoomId &roomId) const
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "PostgreSQL snapshot lookup requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{
        impl_->mutex};

    impl_->ensure_ready_locked();

    ResultPtr result =
        execute_parameters(
            impl_->connection,
            "SELECT "
            "room_version,"
            "last_event_id,"
            "state::text,"
            "schema_version,"
            "created_at_micros,"
            "checksum,"
            "metadata::text "
            "FROM " +
                impl_->qualifiedTable +
                " WHERE room_id = $1 "
                "ORDER BY room_version DESC "
                "LIMIT 1",
            {
                roomId.value(),
            },
            PGRES_TUPLES_OK,
            "failed to load latest PostgreSQL room snapshot");

    if (PQntuples(result.get()) == 0)
    {
      return std::nullopt;
    }

    if (PQntuples(result.get()) != 1)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "latest PostgreSQL snapshot query returned an unexpected row count"};
    }

    return impl_->decode_snapshot_locked(
        roomId,
        result.get(),
        0);

#else

    static_cast<void>(roomId);

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  std::optional<RoomSnapshot>
  PostgresSnapshotStore::load_at_or_before(
      const RoomId &roomId,
      RoomVersion version) const
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "PostgreSQL snapshot lookup requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{
        impl_->mutex};

    impl_->ensure_ready_locked();

    ResultPtr result =
        execute_parameters(
            impl_->connection,
            "SELECT "
            "room_version,"
            "last_event_id,"
            "state::text,"
            "schema_version,"
            "created_at_micros,"
            "checksum,"
            "metadata::text "
            "FROM " +
                impl_->qualifiedTable +
                " WHERE room_id = $1 "
                "AND room_version <= $2 "
                "ORDER BY room_version DESC "
                "LIMIT 1",
            {
                roomId.value(),
                std::to_string(
                    version.value()),
            },
            PGRES_TUPLES_OK,
            "failed to load PostgreSQL room snapshot by version");

    if (PQntuples(result.get()) == 0)
    {
      return std::nullopt;
    }

    if (PQntuples(result.get()) != 1)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "PostgreSQL snapshot query returned an unexpected row count"};
    }

    return impl_->decode_snapshot_locked(
        roomId,
        result.get(),
        0);

#else

    static_cast<void>(roomId);
    static_cast<void>(version);

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  std::vector<RoomSnapshot>
  PostgresSnapshotStore::load_recent(
      const RoomId &roomId,
      std::size_t limit) const
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "PostgreSQL snapshot lookup requires a room identifier"};
    }

    if (limit == 0)
    {
      return {};
    }

    std::lock_guard<std::mutex> lock{
        impl_->mutex};

    impl_->ensure_ready_locked();

    ResultPtr result =
        execute_parameters(
            impl_->connection,
            "SELECT "
            "room_version,"
            "last_event_id,"
            "state::text,"
            "schema_version,"
            "created_at_micros,"
            "checksum,"
            "metadata::text "
            "FROM " +
                impl_->qualifiedTable +
                " WHERE room_id = $1 "
                "ORDER BY room_version DESC "
                "LIMIT $2",
            {
                roomId.value(),
                size_parameter(limit),
            },
            PGRES_TUPLES_OK,
            "failed to load recent PostgreSQL room snapshots");

    const int rowCount =
        PQntuples(result.get());

    std::vector<RoomSnapshot> snapshots;
    snapshots.reserve(
        static_cast<std::size_t>(
            rowCount));

    std::optional<RoomVersion> previousVersion;

    for (int row = 0;
         row < rowCount;
         ++row)
    {
      RoomSnapshot snapshot =
          impl_->decode_snapshot_locked(
              roomId,
              result.get(),
              row);

      if (previousVersion &&
          snapshot.room_version() >=
              *previousVersion)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "PostgreSQL snapshots are not ordered by descending version"};
      }

      previousVersion =
          snapshot.room_version();

      snapshots.push_back(
          std::move(snapshot));
    }

    return snapshots;

#else

    static_cast<void>(roomId);
    static_cast<void>(limit);

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  std::size_t PostgresSnapshotStore::count(
      const RoomId &roomId) const
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "PostgreSQL snapshot count requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{
        impl_->mutex};

    impl_->ensure_ready_locked();

    ResultPtr result =
        execute_parameters(
            impl_->connection,
            "SELECT COUNT(*) "
            "FROM " +
                impl_->qualifiedTable +
                " WHERE room_id = $1",
            {
                roomId.value(),
            },
            PGRES_TUPLES_OK,
            "failed to count PostgreSQL room snapshots");

    if (PQntuples(result.get()) != 1)
    {
      throw Error{
          ErrorCode::SnapshotStoreFailure,
          "PostgreSQL snapshot count returned an unexpected row count"};
    }

    const std::uint64_t value =
        parse_u64(
            result_value(
                result.get(),
                0,
                0,
                "count"),
            "count");

    if (value >
        std::numeric_limits<std::size_t>::max())
    {
      throw Error{
          ErrorCode::SnapshotStoreFailure,
          "PostgreSQL snapshot count exceeds platform size limits"};
    }

    return static_cast<std::size_t>(
        value);

#else

    static_cast<void>(roomId);

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  std::size_t PostgresSnapshotStore::prune(
      const RoomId &roomId,
      std::size_t keep)
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "PostgreSQL snapshot pruning requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{
        impl_->mutex};

    impl_->ensure_ready_locked();

    Transaction transaction{
        impl_->connection};

    impl_->lock_room_locked(roomId);

    ResultPtr result =
        execute_parameters(
            impl_->connection,
            "DELETE FROM " +
                impl_->qualifiedTable +
                " WHERE room_id = $1 "
                "AND room_version IN ("
                "SELECT room_version "
                "FROM " +
                impl_->qualifiedTable +
                " WHERE room_id = $1 "
                "ORDER BY room_version DESC "
                "OFFSET $2"
                ")",
            {
                roomId.value(),
                size_parameter(keep),
            },
            PGRES_COMMAND_OK,
            "failed to prune PostgreSQL room snapshots");

    const std::size_t removed =
        affected_rows(
            result.get());

    transaction.commit();

    return removed;

#else

    static_cast<void>(roomId);
    static_cast<void>(keep);

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  bool PostgresSnapshotStore::clear_room(
      const RoomId &roomId)
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "PostgreSQL snapshot removal requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{
        impl_->mutex};

    impl_->ensure_ready_locked();

    Transaction transaction{
        impl_->connection};

    impl_->lock_room_locked(roomId);

    ResultPtr result =
        execute_parameters(
            impl_->connection,
            "DELETE FROM " +
                impl_->qualifiedTable +
                " WHERE room_id = $1",
            {
                roomId.value(),
            },
            PGRES_COMMAND_OK,
            "failed to clear PostgreSQL room snapshots");

    const bool removed =
        affected_rows(
            result.get()) != 0;

    transaction.commit();

    return removed;

#else

    static_cast<void>(roomId);

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  bool PostgresSnapshotStore::ping() const noexcept
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    try
    {
      std::lock_guard<std::mutex> lock{
          impl_->mutex};

      impl_->ensure_connected_locked();

      ResultPtr result{
          PQexec(
              impl_->connection,
              "SELECT 1")};

      return result &&
             PQresultStatus(result.get()) ==
                 PGRES_TUPLES_OK &&
             PQntuples(result.get()) == 1;
    }
    catch (...)
    {
      return false;
    }

#else

    return false;

#endif
  }

  const PostgresSnapshotStoreOptions &
  PostgresSnapshotStore::options() const noexcept
  {
    return impl_->options;
  }

  bool PostgresSnapshotStore::
      compiled_with_postgres() noexcept
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    return true;

#else

    return false;

#endif
  }

} // namespace vix::realtime
