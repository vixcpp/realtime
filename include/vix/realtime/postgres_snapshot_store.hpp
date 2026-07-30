/**
 *
 * @file postgres_snapshot_store.hpp
 * @author Gaspard Kirira
 * @brief PostgreSQL-backed snapshot store for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_POSTGRES_SNAPSHOT_STORE_HPP
#define VIX_REALTIME_POSTGRES_SNAPSHOT_STORE_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/snapshot_store.hpp>

namespace vix::realtime
{
  /**
   * @brief Configuration for the PostgreSQL snapshot store.
   */
  struct PostgresSnapshotStoreOptions
  {
    /**
     * @brief PostgreSQL connection string.
     *
     * The value uses the standard libpq connection string format.
     */
    std::string connectionString{};

    /** @brief PostgreSQL schema containing the snapshot table. */
    std::string schema{"public"};

    /** @brief PostgreSQL table containing room snapshots. */
    std::string table{"vix_realtime_snapshots"};

    /**
     * @brief Create the configured schema when it does not exist.
     */
    bool createSchemaIfMissing{false};

    /**
     * @brief Create the snapshot table when it does not exist.
     */
    bool createTableIfMissing{true};

    /**
     * @brief Attempt to reset a failed PostgreSQL connection.
     */
    bool reconnect{true};

    /**
     * @brief Validate PostgreSQL snapshot store configuration.
     *
     * @throws vix::realtime::Error
     *         When the connection string or SQL identifiers are invalid.
     */
    void validate() const;
  };

  /**
   * @brief PostgreSQL implementation of the Realtime snapshot store.
   *
   * Snapshots are uniquely identified by room ID and room version. Saving an
   * existing version replaces its serialized state only when the stored and
   * supplied snapshots reference the same last event identifier.
   *
   * One store instance owns one libpq connection and serializes operations on
   * that connection.
   */
  class VIX_REALTIME_API PostgresSnapshotStore final
      : public SnapshotStore
  {
  public:
    /**
     * @brief Construct a PostgreSQL snapshot store from a connection string.
     *
     * @param connectionString PostgreSQL libpq connection string.
     *
     * @throws vix::realtime::Error
     *         When PostgreSQL support is unavailable or connection fails.
     */
    explicit PostgresSnapshotStore(
        std::string connectionString);

    /**
     * @brief Construct a configured PostgreSQL snapshot store.
     *
     * @param options PostgreSQL snapshot store configuration.
     *
     * @throws vix::realtime::Error
     *         When configuration, connection, or initialization fails.
     */
    explicit PostgresSnapshotStore(
        PostgresSnapshotStoreOptions options);

    /**
     * @brief Destroy the PostgreSQL connection.
     */
    ~PostgresSnapshotStore() override;

    PostgresSnapshotStore(
        const PostgresSnapshotStore &) = delete;

    PostgresSnapshotStore &operator=(
        const PostgresSnapshotStore &) = delete;

    PostgresSnapshotStore(
        PostgresSnapshotStore &&) = delete;

    PostgresSnapshotStore &operator=(
        PostgresSnapshotStore &&) = delete;

    /**
     * @brief Persist or replace one room snapshot.
     *
     * An existing snapshot may be replaced only when its last event identifier
     * matches the supplied snapshot.
     *
     * @param snapshot Snapshot to persist.
     * @return Persisted snapshot.
     */
    [[nodiscard]] RoomSnapshot save(
        RoomSnapshot snapshot) override;

    /**
     * @brief Load the newest snapshot belonging to a room.
     *
     * @param roomId Room identifier.
     * @return Latest snapshot, or no value when none exists.
     */
    [[nodiscard]] std::optional<RoomSnapshot>
    load_latest(
        const RoomId &roomId) const override;

    /**
     * @brief Load the newest snapshot at or before a room version.
     *
     * @param roomId Room identifier.
     * @param version Maximum accepted room version.
     * @return Matching snapshot, or no value.
     */
    [[nodiscard]] std::optional<RoomSnapshot>
    load_at_or_before(
        const RoomId &roomId,
        RoomVersion version) const override;

    /**
     * @brief Load recent room snapshots from newest to oldest.
     *
     * @param roomId Room identifier.
     * @param limit Maximum number of snapshots to return.
     * @return Snapshots ordered by descending room version.
     */
    [[nodiscard]] std::vector<RoomSnapshot>
    load_recent(
        const RoomId &roomId,
        std::size_t limit) const override;

    /**
     * @brief Return the number of stored snapshots for a room.
     *
     * @param roomId Room identifier.
     * @return Snapshot count.
     */
    [[nodiscard]] std::size_t count(
        const RoomId &roomId) const override;

    /**
     * @brief Retain only the newest snapshots for a room.
     *
     * @param roomId Room identifier.
     * @param keep Number of newest snapshots to preserve.
     * @return Number of deleted snapshots.
     */
    std::size_t prune(
        const RoomId &roomId,
        std::size_t keep) override;

    /**
     * @brief Delete every snapshot belonging to a room.
     *
     * @param roomId Room identifier.
     * @return True when at least one snapshot was deleted.
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
     * @return Snapshot store configuration.
     */
    [[nodiscard]] const PostgresSnapshotStoreOptions &
    options() const noexcept;

    /**
     * @brief Return whether PostgreSQL support was compiled.
     *
     * @return True when the libpq implementation is available.
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
   * @brief Shared ownership pointer for a PostgreSQL snapshot store.
   */
  using PostgresSnapshotStorePtr =
      std::shared_ptr<PostgresSnapshotStore>;

} // namespace vix::realtime

#endif // VIX_REALTIME_POSTGRES_SNAPSHOT_STORE_HPP
