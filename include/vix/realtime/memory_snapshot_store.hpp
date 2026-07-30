/**
 *
 * @file memory_snapshot_store.hpp
 * @author Gaspard Kirira
 * @brief Thread-safe in-memory snapshot store for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_MEMORY_SNAPSHOT_STORE_HPP
#define VIX_REALTIME_MEMORY_SNAPSHOT_STORE_HPP

#include <cstddef>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/snapshot_store.hpp>

namespace vix::realtime
{
  /**
   * @brief Thread-safe snapshot store backed by process memory.
   *
   * Snapshots are stored independently for each room and remain ordered by
   * ascending room version.
   *
   * This implementation is intended for tests, examples, development, and
   * single-process applications that do not require durable persistence.
   */
  class VIX_REALTIME_API MemorySnapshotStore final
      : public SnapshotStore
  {
  public:
    /**
     * @brief Construct an empty in-memory snapshot store.
     */
    MemorySnapshotStore() = default;

    /**
     * @brief Destroy the snapshot store.
     */
    ~MemorySnapshotStore() override = default;

    /**
     * @brief Persist a room snapshot in memory.
     *
     * @param snapshot Snapshot to persist.
     * @return Persisted snapshot.
     */
    [[nodiscard]] RoomSnapshot save(
        RoomSnapshot snapshot) override;

    /**
     * @brief Load the latest snapshot of a room.
     *
     * @param roomId Room snapshot stream to inspect.
     * @return Latest snapshot, or no value when none exists.
     */
    [[nodiscard]] std::optional<RoomSnapshot>
    load_latest(const RoomId &roomId) const override;

    /**
     * @brief Load the newest snapshot at or before a room version.
     *
     * @param roomId Room snapshot stream to inspect.
     * @param version Maximum accepted room version.
     * @return Matching snapshot, or no value when none exists.
     */
    [[nodiscard]] std::optional<RoomSnapshot>
    load_at_or_before(
        const RoomId &roomId,
        RoomVersion version) const override;

    /**
     * @brief Load recent snapshots from newest to oldest.
     *
     * @param roomId Room snapshot stream to inspect.
     * @param limit Maximum number of snapshots to return.
     * @return Recent snapshots.
     */
    [[nodiscard]] std::vector<RoomSnapshot>
    load_recent(
        const RoomId &roomId,
        std::size_t limit) const override;

    /**
     * @brief Return the number of snapshots stored for a room.
     *
     * @param roomId Room snapshot stream to inspect.
     * @return Snapshot count.
     */
    [[nodiscard]] std::size_t count(
        const RoomId &roomId) const override;

    /**
     * @brief Retain only the newest snapshots of a room.
     *
     * @param roomId Room snapshot stream to prune.
     * @param keep Number of newest snapshots to retain.
     * @return Number of removed snapshots.
     */
    std::size_t prune(
        const RoomId &roomId,
        std::size_t keep) override;

    /**
     * @brief Remove every snapshot belonging to a room.
     *
     * @param roomId Room snapshot stream to remove.
     * @return True when an existing stream was removed.
     */
    bool clear_room(
        const RoomId &roomId) override;

    /**
     * @brief Remove every snapshot stream.
     */
    void clear();

    /**
     * @brief Return the number of room snapshot streams.
     *
     * @return Room stream count.
     */
    [[nodiscard]] std::size_t room_count() const;

  private:
    /**
     * @brief Ordered snapshot history for one room.
     */
    using SnapshotStream = std::vector<RoomSnapshot>;

    /**
     * @brief Validate a snapshot against an existing stream.
     *
     * @param snapshot Snapshot to validate.
     * @param stream Existing ordered room snapshot stream.
     *
     * @throws vix::realtime::Error
     *         When the snapshot is stale or inconsistent.
     */
    static void validate_save(
        const RoomSnapshot &snapshot,
        const SnapshotStream &stream);

    /** @brief Protects every in-memory snapshot stream. */
    mutable std::mutex mutex_{};

    /** @brief Snapshot streams indexed by room identifier. */
    std::unordered_map<RoomId, SnapshotStream> streams_{};
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_MEMORY_SNAPSHOT_STORE_HPP
