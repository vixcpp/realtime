/**
 *
 * @file snapshot_policy.hpp
 * @author Gaspard Kirira
 * @brief Snapshot scheduling policy for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_INTERNAL_SNAPSHOT_POLICY_HPP
#define VIX_REALTIME_INTERNAL_SNAPSHOT_POLICY_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <vix/realtime/config.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_version.hpp>

namespace vix::realtime::internal
{
  /**
   * @brief Reason for creating a room snapshot.
   */
  enum class SnapshotReason : std::uint8_t
  {
    /** @brief No snapshot is currently required. */
    None = 0,

    /** @brief The configured event interval was reached. */
    EventInterval,

    /** @brief The room is closing and close snapshots are enabled. */
    RoomClose,

    /** @brief Snapshot creation was explicitly requested. */
    Explicit
  };

  /**
   * @brief Return the stable textual representation of a snapshot reason.
   *
   * @param reason Snapshot reason.
   * @return Stable lowercase identifier.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(SnapshotReason reason) noexcept
  {
    switch (reason)
    {
    case SnapshotReason::None:
      return "none";

    case SnapshotReason::EventInterval:
      return "event_interval";

    case SnapshotReason::RoomClose:
      return "room_close";

    case SnapshotReason::Explicit:
      return "explicit";
    }

    return "none";
  }

  /**
   * @brief Result of evaluating the snapshot policy.
   */
  struct SnapshotDecision
  {
    /** @brief Whether a new snapshot should be created. */
    bool required{false};

    /** @brief Reason for the decision. */
    SnapshotReason reason{SnapshotReason::None};

    /** @brief Number of events since the latest snapshot. */
    std::size_t eventsSinceSnapshot{0};

    /** @brief Number of recent snapshots that should be retained. */
    std::size_t snapshotsToKeep{0};

    /**
     * @brief Return whether snapshot creation is required.
     *
     * @return True when a snapshot should be created.
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
      return required;
    }
  };

  /**
   * @brief Determines when room snapshots should be created and retained.
   *
   * The policy is stateless and may be shared by multiple rooms. It evaluates
   * the current room position against the latest persisted snapshot.
   *
   * Snapshot creation may be triggered by:
   *
   * - an explicit runtime request;
   * - room closure when close snapshots are enabled;
   * - reaching the configured number of events since the latest snapshot.
   */
  class SnapshotPolicy
  {
  public:
    /**
     * @brief Construct a snapshot policy.
     *
     * @param everyEvents Number of events between automatic snapshots.
     *                    Zero disables interval snapshots.
     * @param snapshotsToKeep Number of recent snapshots to retain.
     * @param snapshotOnRoomClose Whether room closure triggers a snapshot.
     *
     * @throws vix::realtime::Error
     *         When snapshot retention is zero.
     */
    SnapshotPolicy(
        std::size_t everyEvents,
        std::size_t snapshotsToKeep,
        bool snapshotOnRoomClose);

    /**
     * @brief Construct a snapshot policy from Realtime configuration.
     *
     * @param config Realtime runtime configuration.
     * @return Configured snapshot policy.
     *
     * @throws vix::realtime::Error
     *         When the snapshot configuration is invalid.
     */
    [[nodiscard]] static SnapshotPolicy from_config(
        const Config &config);

    /**
     * @brief Evaluate whether a room snapshot should be created.
     *
     * The room version and event identifier describe the current authoritative
     * state after all newly persisted events have been applied.
     *
     * When a latest snapshot is supplied, it must belong to the same logical
     * stream position and must not be ahead of the current room.
     *
     * @param roomVersion Current room version.
     * @param lastEventId Current last persisted event identifier.
     * @param latestSnapshot Latest stored snapshot, or null when none exists.
     * @param roomClosing Whether the room is currently closing.
     * @param explicitRequest Whether snapshot creation was explicitly requested.
     * @return Snapshot policy decision.
     *
     * @throws vix::realtime::Error
     *         When room and snapshot positions are inconsistent.
     */
    [[nodiscard]] SnapshotDecision evaluate(
        RoomVersion roomVersion,
        EventId lastEventId,
        const RoomSnapshot *latestSnapshot = nullptr,
        bool roomClosing = false,
        bool explicitRequest = false) const;

    /**
     * @brief Return the number of events between automatic snapshots.
     *
     * A value of zero means interval snapshots are disabled.
     *
     * @return Automatic snapshot event interval.
     */
    [[nodiscard]] std::size_t every_events() const noexcept;

    /**
     * @brief Return the number of recent snapshots to retain.
     *
     * @return Snapshot retention count.
     */
    [[nodiscard]] std::size_t snapshots_to_keep() const noexcept;

    /**
     * @brief Return whether room closure triggers a snapshot.
     *
     * @return True when close snapshots are enabled.
     */
    [[nodiscard]] bool snapshot_on_room_close() const noexcept;

    /**
     * @brief Return whether interval snapshots are enabled.
     *
     * @return True when the event interval is greater than zero.
     */
    [[nodiscard]] bool interval_enabled() const noexcept;

  private:
    /** @brief Number of events between automatic snapshots. */
    std::size_t everyEvents_{0};

    /** @brief Number of recent snapshots retained after saving. */
    std::size_t snapshotsToKeep_{1};

    /** @brief Whether room closure triggers snapshot creation. */
    bool snapshotOnRoomClose_{false};
  };

} // namespace vix::realtime::internal

#endif // VIX_REALTIME_INTERNAL_SNAPSHOT_POLICY_HPP
