/**
 *
 * @file postgres_event_store.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the PostgreSQL Vix Realtime event store.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/postgres_event_store.hpp>

#include <algorithm>
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
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/types.hpp>

#if (defined(VIX_REALTIME_WITH_POSTGRES) && VIX_REALTIME_WITH_POSTGRES) || \
    defined(VIX_REALTIME_ENABLE_POSTGRES) || \
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
      const auto micros =
          std::chrono::duration_cast<
              std::chrono::microseconds>(
              value.time_since_epoch())
              .count();

      if constexpr (
          sizeof(decltype(micros)) >
          sizeof(std::int64_t))
      {
        if (micros >
                std::numeric_limits<std::int64_t>::max() ||
            micros <
                std::numeric_limits<std::int64_t>::min())
        {
          throw Error{
              ErrorCode::EventStoreFailure,
              "room event timestamp exceeds PostgreSQL storage range"};
        }
      }

      return static_cast<std::int64_t>(
          micros);
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
                "invalid PostgreSQL integer field: "} +
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
                "invalid PostgreSQL unsigned field: "} +
                std::string{field}};
      }

      return result;
    }

    /**
     * @brief Parse one schema version.
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
            "PostgreSQL event contains an invalid schema version"};
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
            "PostgreSQL event JSON value must be an object"};
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
              "PostgreSQL event JSON integer exceeds signed 64-bit range"};
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
          "PostgreSQL event contains an unsupported JSON value"};
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
                "failed to parse PostgreSQL event "} +
                std::string{field} +
                ": " +
                error.what()};
      }
      catch (...)
      {
        throw Error{
            ErrorCode::CorruptedState,
            std::string{
                "failed to parse PostgreSQL event "} +
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
            ErrorCode::EventStoreFailure,
            std::string{
                "failed to serialize room event JSON: "} +
                error.what()};
      }
      catch (...)
      {
        throw Error{
            ErrorCode::EventStoreFailure,
            "failed to serialize room event JSON"};
      }
    }

  } // namespace

  void PostgresEventStoreOptions::validate() const
  {
    if (connectionString.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "PostgreSQL event store requires a connection string"};
    }

    if (!valid_identifier(schema))
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "PostgreSQL event store schema name is invalid"};
    }

    if (!valid_identifier(table))
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "PostgreSQL event store table name is invalid"};
    }
  }

#if VIX_REALTIME_POSTGRES_COMPILED

  namespace
  {
    /**
     * @brief Automatically clears a libpq result.
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
      ResultPtr result{
          PQexec(
              connection,
              std::string{sql}.c_str())};

      if (!result ||
          PQresultStatus(result.get()) !=
              PGRES_COMMAND_OK)
      {
        throw Error{
            ErrorCode::EventStoreFailure,
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
            ErrorCode::EventStoreFailure,
            "PostgreSQL parameter count exceeds libpq limits"};
      }

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
              std::string{sql}.c_str(),
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
            ErrorCode::EventStoreFailure,
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
                "failed to begin PostgreSQL event transaction"));
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

      Transaction(const Transaction &) = delete;
      Transaction &operator=(const Transaction &) = delete;

      void commit()
      {
        static_cast<void>(
            execute_command(
                connection_,
                "COMMIT",
                "failed to commit PostgreSQL event transaction"));

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
                "PostgreSQL event field is null: "} +
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

  } // namespace

  struct PostgresEventStore::Impl
  {
    explicit Impl(
        PostgresEventStoreOptions configuredOptions)
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
                  "failed to connect PostgreSQL event store");

          if (connection != nullptr)
          {
            PQfinish(connection);
            connection = nullptr;
          }

          throw Error{
              ErrorCode::EventStoreFailure,
              message};
        }

        if (PQsetClientEncoding(
                connection,
                "UTF8") != 0)
        {
          throw Error{
              ErrorCode::EventStoreFailure,
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
            ErrorCode::EventStoreFailure,
            postgres_error(
                connection,
                nullptr,
                "PostgreSQL event store connection is unavailable")};
      }

      PQreset(connection);

      if (PQstatus(connection) !=
          CONNECTION_OK)
      {
        throw Error{
            ErrorCode::EventStoreFailure,
            postgres_error(
                connection,
                nullptr,
                "failed to reconnect PostgreSQL event store")};
      }

      if (PQsetClientEncoding(
              connection,
              "UTF8") != 0)
      {
        throw Error{
            ErrorCode::EventStoreFailure,
            postgres_error(
                connection,
                nullptr,
                "failed to restore PostgreSQL UTF-8 encoding")};
      }
    }

    /**
     * @brief Initialize the configured PostgreSQL schema and table.
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
                "failed to create PostgreSQL event schema"));
      }

      if (options.createTableIfMissing)
      {
        const std::string sql =
            "CREATE TABLE IF NOT EXISTS " +
            qualifiedTable +
            " ("
            "room_id TEXT NOT NULL,"
            "event_id BIGINT NOT NULL CHECK (event_id > 0),"
            "room_version BIGINT NOT NULL CHECK (room_version > 0),"
            "type TEXT NOT NULL,"
            "payload JSONB NOT NULL,"
            "audience SMALLINT NOT NULL,"
            "target_session_id TEXT NULL,"
            "source_session_id TEXT NULL,"
            "request_id TEXT NOT NULL DEFAULT '',"
            "correlation_id TEXT NOT NULL DEFAULT '',"
            "schema_version INTEGER NOT NULL "
            "CHECK (schema_version > 0),"
            "created_at_micros BIGINT NOT NULL,"
            "metadata JSONB NOT NULL,"
            "PRIMARY KEY (room_id, event_id),"
            "UNIQUE (room_id, room_version)"
            ")";

        static_cast<void>(
            execute_command(
                connection,
                sql,
                "failed to create PostgreSQL event table"));
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
              "hashtextextended($1, 0))",
              {
                  roomId.value(),
              },
              PGRES_TUPLES_OK,
              "failed to lock PostgreSQL room event stream"));
    }

    /**
     * @brief Return the latest event ID and room version.
     */
    [[nodiscard]] std::pair<EventId, RoomVersion>
    latest_position_locked(
        const RoomId &roomId) const
    {
      ResultPtr result =
          execute_parameters(
              connection,
              "SELECT "
              "COALESCE(MAX(event_id), 0), "
              "COALESCE(MAX(room_version), 0) "
              "FROM " +
                  qualifiedTable +
                  " WHERE room_id = $1",
              {
                  roomId.value(),
              },
              PGRES_TUPLES_OK,
              "failed to load PostgreSQL room event position");

      if (PQntuples(result.get()) != 1)
      {
        throw Error{
            ErrorCode::EventStoreFailure,
            "PostgreSQL event position query returned an unexpected row count"};
      }

      const EventIdValue eventId =
          parse_i64(
              result_value(
                  result.get(),
                  0,
                  0,
                  "event_id"),
              "event_id");

      const VersionValue roomVersion =
          parse_i64(
              result_value(
                  result.get(),
                  0,
                  1,
                  "room_version"),
              "room_version");

      return {
          EventId{eventId},
          RoomVersion{roomVersion}};
    }

    /**
     * @brief Return the room version at a specific event cursor.
     */
    [[nodiscard]] RoomVersion cursor_version_locked(
        const RoomId &roomId,
        EventId eventId) const
    {
      if (eventId.empty())
      {
        return {};
      }

      ResultPtr result =
          execute_parameters(
              connection,
              "SELECT room_version "
              "FROM " +
                  qualifiedTable +
                  " WHERE room_id = $1 "
                  "AND event_id = $2",
              {
                  roomId.value(),
                  std::to_string(
                      eventId.value()),
              },
              PGRES_TUPLES_OK,
              "failed to resolve PostgreSQL replay cursor");

      if (PQntuples(result.get()) == 0)
      {
        throw Error{
            ErrorCode::ReplayUnavailable,
            "PostgreSQL replay cursor does not exist"};
      }

      if (PQntuples(result.get()) != 1)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "PostgreSQL replay cursor is not unique"};
      }

      return RoomVersion{
          parse_i64(
              result_value(
                  result.get(),
                  0,
                  0,
                  "room_version"),
              "room_version")};
    }

    /**
     * @brief Decode one PostgreSQL event row.
     */
    [[nodiscard]] RoomEvent decode_event_locked(
        const RoomId &roomId,
        PGresult *result,
        int row) const
    {
      const EventId eventId{
          parse_i64(
              result_value(
                  result,
                  row,
                  0,
                  "event_id"),
              "event_id")};

      const RoomVersion roomVersion{
          parse_i64(
              result_value(
                  result,
                  row,
                  1,
                  "room_version"),
              "room_version")};

      const std::string type{
          result_value(
              result,
              row,
              2,
              "type")};

      JsonObject payload =
          parse_json_object(
              result_value(
                  result,
                  row,
                  3,
                  "payload"),
              "payload");

      const std::int64_t audienceValue =
          parse_i64(
              result_value(
                  result,
                  row,
                  4,
                  "audience"),
              "audience");

      if (audienceValue <
              static_cast<std::int64_t>(
                  EventAudience::Room) ||
          audienceValue >
              static_cast<std::int64_t>(
                  EventAudience::Internal))
      {
        throw Error{
            ErrorCode::CorruptedState,
            "PostgreSQL event contains an invalid audience"};
      }

      const EventAudience audience =
          static_cast<EventAudience>(
              audienceValue);

      const std::optional<std::string> targetSession =
          optional_result_value(
              result,
              row,
              5);

      const std::optional<std::string> sourceSession =
          optional_result_value(
              result,
              row,
              6);

      const std::string requestId{
          result_value(
              result,
              row,
              7,
              "request_id")};

      const std::string correlationId{
          result_value(
              result,
              row,
              8,
              "correlation_id")};

      const SchemaVersion schemaVersion =
          parse_schema_version(
              result_value(
                  result,
                  row,
                  9,
                  "schema_version"));

      const Timestamp createdAt =
          timestamp_from_micros(
              parse_i64(
                  result_value(
                      result,
                      row,
                      10,
                      "created_at_micros"),
                  "created_at_micros"));

      JsonObject metadata =
          parse_json_object(
              result_value(
                  result,
                  row,
                  11,
                  "metadata"),
              "metadata");

      /*
       * Construct with room audience first because session-targeted events
       * require their target before final event validation.
       */
      RoomEvent event{
          roomId,
          type,
          std::move(payload),
          EventAudience::Room};

      if (targetSession)
      {
        event.set_target_session(
            SessionId{
                *targetSession});
      }

      if (sourceSession)
      {
        event.set_source_session(
            SessionId{
                *sourceSession});
      }

      event
          .set_audience(audience)
          .set_event_id(eventId)
          .set_room_version(roomVersion)
          .set_request_id(requestId)
          .set_correlation_id(correlationId)
          .set_schema_version(schemaVersion)
          .set_created_at(createdAt)
          .set_metadata(std::move(metadata));

      event.validate();
      return event;
    }

    /** @brief PostgreSQL event store configuration. */
    PostgresEventStoreOptions options{};

    /** @brief Fully qualified and quoted event table name. */
    std::string qualifiedTable{};

    /** @brief Serializes access to the single libpq connection. */
    mutable std::mutex mutex{};

    /** @brief Owned libpq connection. */
    mutable PGconn *connection{nullptr};

    /** @brief Whether optional schema initialization completed. */
    mutable bool initialized{false};
  };

#else

  struct PostgresEventStore::Impl
  {
    explicit Impl(
        PostgresEventStoreOptions configuredOptions)
        : options(std::move(configuredOptions))
    {
    }

    PostgresEventStoreOptions options{};
  };

#endif

  PostgresEventStore::PostgresEventStore(
      std::string connectionString)
      : PostgresEventStore(
            PostgresEventStoreOptions{
                std::move(connectionString)})
  {
  }

  PostgresEventStore::PostgresEventStore(
      PostgresEventStoreOptions options)
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

  PostgresEventStore::~PostgresEventStore() = default;

  RoomEvent PostgresEventStore::append(
      RoomEvent event)
  {
    std::vector<RoomEvent> persisted =
        append_batch(
            {std::move(event)});

    return std::move(
        persisted.front());
  }

  std::vector<RoomEvent>
  PostgresEventStore::append_batch(
      std::vector<RoomEvent> events)
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    if (events.empty())
    {
      return {};
    }

    const RoomId roomId =
        events.front().room_id();

    if (roomId.empty())
    {
      throw Error{
          ErrorCode::EventStoreFailure,
          "PostgreSQL event append requires a room identifier"};
    }

    RoomVersion previousVersion{};

    for (std::size_t index = 0;
         index < events.size();
         ++index)
    {
      RoomEvent &event =
          events[index];

      event.validate();

      if (event.room_id() != roomId)
      {
        throw Error{
            ErrorCode::EventStoreFailure,
            "PostgreSQL event batch contains multiple room identifiers"};
      }

      if (!event.event_id().empty())
      {
        throw Error{
            ErrorCode::EventStoreFailure,
            "PostgreSQL event append cannot accept a preassigned event identifier"};
      }

      if (index != 0 &&
          event.room_version() !=
              previousVersion.next())
      {
        throw Error{
            ErrorCode::EventStoreFailure,
            "PostgreSQL event batch contains non-contiguous room versions"};
      }

      previousVersion =
          event.room_version();
    }

    std::lock_guard<std::mutex> lock{
        impl_->mutex};

    impl_->ensure_ready_locked();

    Transaction transaction{
        impl_->connection};

    impl_->lock_room_locked(roomId);

    auto [lastEventId, lastRoomVersion] =
        impl_->latest_position_locked(
            roomId);

    if (events.front().room_version() !=
        lastRoomVersion.next())
    {
      throw Error{
          ErrorCode::EventStoreFailure,
          "PostgreSQL event room version does not follow the stored stream"};
    }

    const std::string insertSql =
        "INSERT INTO " +
        impl_->qualifiedTable +
        " ("
        "room_id,"
        "event_id,"
        "room_version,"
        "type,"
        "payload,"
        "audience,"
        "target_session_id,"
        "source_session_id,"
        "request_id,"
        "correlation_id,"
        "schema_version,"
        "created_at_micros,"
        "metadata"
        ") VALUES ("
        "$1,$2,$3,$4,$5::jsonb,$6,$7,$8,$9,$10,$11,$12,$13::jsonb"
        ")";

    for (auto &event : events)
    {
      lastEventId.increment();
      event.set_event_id(lastEventId);
      event.validate();

      std::optional<std::string> targetSession;

      if (event.target_session())
      {
        targetSession =
            event.target_session()->value();
      }

      std::optional<std::string> sourceSession;

      if (event.source_session())
      {
        sourceSession =
            event.source_session()->value();
      }

      static_cast<void>(
          execute_parameters(
              impl_->connection,
              insertSql,
              {
                  roomId.value(),
                  std::to_string(
                      event.event_id().value()),
                  std::to_string(
                      event.room_version().value()),
                  event.type(),
                  serialize_json_object(
                      event.payload()),
                  std::to_string(
                      static_cast<std::uint8_t>(
                          event.audience())),
                  std::move(targetSession),
                  std::move(sourceSession),
                  event.request_id(),
                  event.correlation_id(),
                  std::to_string(
                      event.schema_version()),
                  std::to_string(
                      timestamp_to_micros(
                          event.created_at())),
                  serialize_json_object(
                      event.metadata()),
              },
              PGRES_COMMAND_OK,
              "failed to append PostgreSQL room event"));
    }

    transaction.commit();
    return events;

#else

    static_cast<void>(events);

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  std::vector<RoomEvent>
  PostgresEventStore::load_after(
      const RoomId &roomId,
      EventId after,
      std::size_t limit) const
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "PostgreSQL event replay requires a room identifier"};
    }

    if (limit == 0)
    {
      return {};
    }

    std::lock_guard<std::mutex> lock{
        impl_->mutex};

    impl_->ensure_ready_locked();

    const auto [latestEventId, latestVersion] =
        impl_->latest_position_locked(
            roomId);

    static_cast<void>(latestVersion);

    if (after > latestEventId)
    {
      throw Error{
          ErrorCode::ReplayUnavailable,
          "PostgreSQL replay cursor is ahead of the room stream"};
    }

    RoomVersion cursorVersion =
        impl_->cursor_version_locked(
            roomId,
            after);

    ResultPtr result =
        execute_parameters(
            impl_->connection,
            "SELECT "
            "event_id,"
            "room_version,"
            "type,"
            "payload::text,"
            "audience,"
            "target_session_id,"
            "source_session_id,"
            "request_id,"
            "correlation_id,"
            "schema_version,"
            "created_at_micros,"
            "metadata::text "
            "FROM " +
                impl_->qualifiedTable +
                " WHERE room_id = $1 "
                "AND event_id > $2 "
                "ORDER BY event_id ASC "
                "LIMIT $3",
            {
                roomId.value(),
                std::to_string(
                    after.value()),
                std::to_string(limit),
            },
            PGRES_TUPLES_OK,
            "failed to load PostgreSQL room events");

    const int rowCount =
        PQntuples(result.get());

    std::vector<RoomEvent> events;
    events.reserve(
        static_cast<std::size_t>(
            rowCount));

    EventId expectedEventId =
        after;

    RoomVersion expectedRoomVersion =
        cursorVersion;

    for (int row = 0;
         row < rowCount;
         ++row)
    {
      RoomEvent event =
          impl_->decode_event_locked(
              roomId,
              result.get(),
              row);

      expectedEventId.increment();
      expectedRoomVersion.increment();

      if (event.event_id() !=
              expectedEventId ||
          event.room_version() !=
              expectedRoomVersion)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "PostgreSQL room event stream is not contiguous"};
      }

      events.push_back(
          std::move(event));
    }

    return events;

#else

    static_cast<void>(roomId);
    static_cast<void>(after);
    static_cast<void>(limit);

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  EventId PostgresEventStore::latest_event_id(
      const RoomId &roomId) const
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "PostgreSQL event lookup requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{
        impl_->mutex};

    impl_->ensure_ready_locked();

    return impl_->latest_position_locked(
                    roomId)
        .first;

#else

    static_cast<void>(roomId);

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  std::size_t PostgresEventStore::count(
      const RoomId &roomId) const
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "PostgreSQL event count requires a room identifier"};
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
            "failed to count PostgreSQL room events");

    if (PQntuples(result.get()) != 1)
    {
      throw Error{
          ErrorCode::EventStoreFailure,
          "PostgreSQL event count returned an unexpected row count"};
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
          ErrorCode::EventStoreFailure,
          "PostgreSQL event count exceeds platform size limits"};
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

  bool PostgresEventStore::clear_room(
      const RoomId &roomId)
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "PostgreSQL event removal requires a room identifier"};
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
            "failed to clear PostgreSQL room events");

    const char *affectedText =
        PQcmdTuples(result.get());

    const std::uint64_t affected =
        affectedText == nullptr ||
                *affectedText == '\0'
            ? 0
            : parse_u64(
                  affectedText,
                  "deleted_event_count");

    transaction.commit();

    return affected != 0;

#else

    static_cast<void>(roomId);

    throw Error{
        ErrorCode::MissingDependency,
        "Vix Realtime was compiled without PostgreSQL support"};

#endif
  }

  bool PostgresEventStore::ping() const noexcept
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

  const PostgresEventStoreOptions &
  PostgresEventStore::options() const noexcept
  {
    return impl_->options;
  }

  bool PostgresEventStore::
      compiled_with_postgres() noexcept
  {
#if VIX_REALTIME_POSTGRES_COMPILED

    return true;

#else

    return false;

#endif
  }

} // namespace vix::realtime
