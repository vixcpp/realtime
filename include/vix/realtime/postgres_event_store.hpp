/**
 *
 * @file postgres_event_store.hpp
 * @author Gaspard Kirira
 * @brief PostgreSQL-backed authoritative event store for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_POSTGRES_EVENT_STORE_HPP
#define VIX_REALTIME_POSTGRES_EVENT_STORE_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/event_store.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>

namespace vix::realtime
{
  /**
   * @brief Configuration for the PostgreSQL event store.
   */
  struct PostgresEventStoreOptions
  {
    /**
     * @brief PostgreSQL connection string.
     *
     * The value uses the standard libpq connection string format.
     */
    std::string connectionString{};

    /** @brief PostgreSQL schema containing the event table. */
    std::string schema{"public"};

    /** @brief PostgreSQL table containing authoritative events. */
    std::string table{"vix_realtime_events"};

    /**
     * @brief Create the configured schema when it does not exist.
     */
    bool createSchemaIfMissing{false};

    /**
     * @brief Create the event table when it does not exist.
     */
    bool createTableIfMissing{true};

    /**
     * @brief Attempt to reset a failed PostgreSQL connection.
     */
    bool reconnect{true};

    /**
     * @brief Validate PostgreSQL event store configuration.
     *
     * @throws vix::realtime::Error
     *         When the connection string or SQL identifiers are invalid.
     */
    void validate() const;
  };

  /**
   * @brief PostgreSQL implementation of the authoritative event store.
   *
   * The store assigns event identifiers transactionally per room. Appends use
   * a PostgreSQL advisory transaction lock derived from the room identifier,
   * preserving contiguous event identifiers and room versions across multiple
   * runtime processes using the same table.
   *
   * One store instance owns one libpq connection and serializes operations on
   * that connection. Applications requiring greater database concurrency may
   * create multiple store instances or introduce a connection-pool adapter.
   */
  class VIX_REALTIME_API PostgresEventStore final
      : public EventStore
  {
  public:
    /**
     * @brief Construct a PostgreSQL event store from a connection string.
     *
     * Default schema and table settings are used.
     *
     * @param connectionString PostgreSQL libpq connection string.
     *
     * @throws vix::realtime::Error
     *         When PostgreSQL support is unavailable or connection fails.
     */
    explicit PostgresEventStore(
        std::string connectionString);

    /**
     * @brief Construct a configured PostgreSQL event store.
     *
     * @param options PostgreSQL event store configuration.
     *
     * @throws vix::realtime::Error
     *         When PostgreSQL support, configuration, connection, or automatic
     *         schema initialization fails.
     */
    explicit PostgresEventStore(
        PostgresEventStoreOptions options);

    /**
     * @brief Destroy the PostgreSQL connection.
     */
    ~PostgresEventStore() override;

    PostgresEventStore(
        const PostgresEventStore &) = delete;

    PostgresEventStore &operator=(
        const PostgresEventStore &) = delete;

    PostgresEventStore(
        PostgresEventStore &&) = delete;

    PostgresEventStore &operator=(
        PostgresEventStore &&) = delete;

    /**
     * @brief Append one authoritative room event.
     *
     * The event must not already contain a persistent event identifier. Its
     * room version must immediately follow the latest stored room version.
     *
     * @param event Event to persist.
     * @return Persisted event with its assigned event identifier.
     */
    [[nodiscard]] RoomEvent append(
        RoomEvent event) override;

    /**
     * @brief Atomically append a batch of room events.
     *
     * Every event must belong to the same room and contain contiguous room
     * versions. Either every event is inserted or none is inserted.
     *
     * @param events Events to append.
     * @return Persisted events with assigned event identifiers.
     */
    [[nodiscard]] std::vector<RoomEvent> append_batch(
        std::vector<RoomEvent> events) override;

    /**
     * @brief Load persisted room events after one event identifier.
     *
     * Results are ordered by ascending event identifier.
     *
     * @param roomId Room identifier.
     * @param after Exclusive event cursor.
     * @param limit Maximum number of events to return.
     * @return Persisted events after the cursor.
     */
    [[nodiscard]] std::vector<RoomEvent> load_after(
        const RoomId &roomId,
        EventId after,
        std::size_t limit) const override;

    /**
     * @brief Return the latest persisted event identifier for a room.
     *
     * @param roomId Room identifier.
     * @return Latest event ID, or zero when the room has no events.
     */
    [[nodiscard]] EventId latest_event_id(
        const RoomId &roomId) const override;

    /**
     * @brief Return the number of persisted events for a room.
     *
     * @param roomId Room identifier.
     * @return Persisted event count.
     */
    [[nodiscard]] std::size_t count(
        const RoomId &roomId) const override;

    /**
     * @brief Delete every persisted event belonging to a room.
     *
     * @param roomId Room identifier.
     * @return True when at least one event was deleted.
     */
    bool clear_room(
        const RoomId &roomId) override;

    /**
     * @brief Test the current PostgreSQL connection.
     *
     * @return True when the database responds successfully.
     */
    [[nodiscard]] bool ping() const noexcept;

    /**
     * @brief Return the configured PostgreSQL options.
     *
     * @return Event store configuration.
     */
    [[nodiscard]] const PostgresEventStoreOptions &
    options() const noexcept;

    /**
     * @brief Return whether the module was compiled with PostgreSQL support.
     *
     * @return True when the libpq-backed implementation is available.
     */
    [[nodiscard]] static bool
    compiled_with_postgres() noexcept;

  private:
    /** @brief Hidden libpq connection and synchronization state. */
    struct Impl;

    /** @brief PostgreSQL implementation state. */
    std::unique_ptr<Impl> impl_;
  };

  /**
   * @brief Shared ownership pointer for a PostgreSQL event store.
   */
  using PostgresEventStorePtr =
      std::shared_ptr<PostgresEventStore>;

} // namespace vix::realtime

#endif // VIX_REALTIME_POSTGRES_EVENT_STORE_HPP
