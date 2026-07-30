/**
 *
 * @file snapshot_store.hpp
 * @author Gaspard Kirira
 * @brief Persistent snapshot storage interface for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_SNAPSHOT_STORE_HPP
#define VIX_REALTIME_SNAPSHOT_STORE_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_version.hpp>

namespace vix::realtime
{
  /**
   * @brief Persistence contract for Realtime room snapshots.
   *
   * Snapshots contain a complete serialized room state at a specific room
   * version and event position.
   *
   * During restoration, the runtime loads the latest compatible snapshot and
   * replays events occurring after its `last_event_id()`.
   */
  class VIX_REALTIME_API SnapshotStore
  {
  public:
    /**
     * @brief Destroy the snapshot store.
     */
    virtual ~SnapshotStore() = default;

    /**
     * @brief Persist a room snapshot.
     *
     * Stores should preserve snapshots in ascending room version order.
     * Saving the current latest version again may replace that snapshot when
     * its event position is identical.
     *
     * @param snapshot Snapshot to persist.
     * @return Persisted snapshot.
     *
     * @throws vix::realtime::Error
     *         When validation or persistence fails.
     */
    [[nodiscard]] virtual RoomSnapshot save(
        RoomSnapshot snapshot) = 0;

    /**
     * @brief Load the latest snapshot of a room.
     *
     * @param roomId Room whose latest snapshot should be loaded.
     * @return Latest snapshot, or no value when none exists.
     *
     * @throws vix::realtime::Error
     *         When the snapshot stream cannot be read.
     */
    [[nodiscard]] virtual std::optional<RoomSnapshot>
    load_latest(const RoomId &roomId) const = 0;

    /**
     * @brief Load the newest snapshot at or before a room version.
     *
     * This supports restoration against a bounded historical room position.
     *
     * @param roomId Room whose snapshot should be loaded.
     * @param version Maximum accepted room version.
     * @return Matching snapshot, or no value when none exists.
     *
     * @throws vix::realtime::Error
     *         When the snapshot stream cannot be read.
     */
    [[nodiscard]] virtual std::optional<RoomSnapshot>
    load_at_or_before(
        const RoomId &roomId,
        RoomVersion version) const = 0;

    /**
     * @brief Load the most recent snapshots of a room.
     *
     * Results are returned from newest to oldest.
     *
     * @param roomId Room whose snapshots should be loaded.
     * @param limit Maximum number of snapshots to return.
     * @return Recent snapshots in descending room version order.
     *
     * @throws vix::realtime::Error
     *         When the snapshot stream cannot be read.
     */
    [[nodiscard]] virtual std::vector<RoomSnapshot>
    load_recent(
        const RoomId &roomId,
        std::size_t limit) const = 0;

    /**
     * @brief Return the number of snapshots stored for a room.
     *
     * @param roomId Room snapshot stream to inspect.
     * @return Number of persisted snapshots.
     *
     * @throws vix::realtime::Error
     *         When the snapshot stream cannot be inspected.
     */
    [[nodiscard]] virtual std::size_t count(
        const RoomId &roomId) const = 0;

    /**
     * @brief Retain only the newest snapshots of a room.
     *
     * A keep count of zero removes every snapshot belonging to the room.
     *
     * @param roomId Room snapshot stream to prune.
     * @param keep Number of newest snapshots to retain.
     * @return Number of removed snapshots.
     *
     * @throws vix::realtime::Error
     *         When the snapshot stream cannot be pruned.
     */
    virtual std::size_t prune(
        const RoomId &roomId,
        std::size_t keep) = 0;

    /**
     * @brief Remove every snapshot belonging to a room.
     *
     * This operation is intended for tests, administration, and explicit
     * permanent room deletion.
     *
     * @param roomId Room snapshot stream to remove.
     * @return True when an existing stream was removed.
     *
     * @throws vix::realtime::Error
     *         When the snapshot stream cannot be removed.
     */
    virtual bool clear_room(
        const RoomId &roomId) = 0;
  };

  /**
   * @brief Shared ownership pointer for a snapshot store.
   */
  using SnapshotStorePtr = std::shared_ptr<SnapshotStore>;

} // namespace vix::realtime

#endif // VIX_REALTIME_SNAPSHOT_STORE_HPP
