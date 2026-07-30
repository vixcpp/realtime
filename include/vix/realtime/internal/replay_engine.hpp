/**
 *
 * @file replay_engine.hpp
 * @author Gaspard Kirira
 * @brief Snapshot restoration and event replay for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_INTERNAL_REPLAY_ENGINE_HPP
#define VIX_REALTIME_INTERNAL_REPLAY_ENGINE_HPP

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

#include <vix/realtime/config.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/event_store.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/snapshot_store.hpp>

namespace vix::realtime::internal
{
  /**
   * @brief Limits applied while reconstructing a room state.
   */
  struct ReplayOptions
  {
    /** @brief Maximum number of events applied during one replay. */
    std::size_t maxEvents{1000};

    /** @brief Maximum serialized event bytes processed during replay. */
    std::size_t maxBytes{4U * 1024U * 1024U};

    /** @brief Maximum time spent reconstructing room state. */
    std::chrono::milliseconds timeout{5000};

    /** @brief Whether the latest stored snapshot should be restored. */
    bool restoreLatestSnapshot{true};

    /**
     * @brief Validate replay limits.
     *
     * @throws vix::realtime::Error
     *         When an event, byte, or timeout limit is zero or negative.
     */
    void validate() const;
  };

  /**
   * @brief Result of reconstructing one authoritative room state.
   */
  struct ReplayResult
  {
    /** @brief Final reconstructed room version. */
    RoomVersion roomVersion{};

    /** @brief Last event applied to the reconstructed state. */
    EventId lastEventId{};

    /** @brief Number of events applied after the selected snapshot. */
    std::size_t eventCount{0};

    /** @brief Total serialized bytes represented by replayed events. */
    std::size_t replayBytes{0};

    /** @brief Time spent restoring and replaying the state. */
    std::chrono::milliseconds elapsed{0};

    /** @brief Snapshot used as the replay starting point. */
    std::optional<RoomSnapshot> snapshot{};

    /** @brief Events applied after the snapshot. */
    std::vector<RoomEvent> events{};

    /**
     * @brief Return whether a snapshot was restored.
     *
     * @return True when `snapshot` contains a value.
     */
    [[nodiscard]] bool used_snapshot() const noexcept
    {
      return snapshot.has_value();
    }

    /**
     * @brief Return whether no event had to be replayed.
     *
     * @return True when the replay event collection is empty.
     */
    [[nodiscard]] bool empty() const noexcept
    {
      return events.empty();
    }
  };

  /**
   * @brief Reconstructs room state from snapshots and persisted events.
   *
   * The engine performs replay on a cloned room state. The supplied state is
   * replaced only after snapshot restoration and event application complete
   * successfully.
   *
   * Replay validates:
   *
   * - snapshot room identity and stream position;
   * - contiguous room versions;
   * - contiguous event identifiers;
   * - replay event, byte, and time limits;
   * - consistency with the event store position observed at replay start.
   */
  class ReplayEngine
  {
  public:
    /**
     * @brief Construct a replay engine.
     *
     * @param eventStore Authoritative event store.
     * @param snapshotStore Optional snapshot store.
     * @param options Replay limits and snapshot behavior.
     *
     * @throws vix::realtime::Error
     *         When the event store or replay options are invalid.
     */
    ReplayEngine(
        EventStorePtr eventStore,
        SnapshotStorePtr snapshotStore = nullptr,
        ReplayOptions options = {});

    /**
     * @brief Construct a replay engine from Realtime configuration.
     *
     * @param config Realtime runtime configuration.
     * @param eventStore Authoritative event store.
     * @param snapshotStore Optional snapshot store.
     * @return Configured replay engine.
     */
    [[nodiscard]] static ReplayEngine from_config(
        const Config &config,
        EventStorePtr eventStore,
        SnapshotStorePtr snapshotStore = nullptr);

    /**
     * @brief Restore one room using its latest available snapshot.
     *
     * Snapshot restoration depends on `restore_latest_snapshot()`. When no
     * snapshot is selected, replay begins at room version zero and event ID
     * zero using the cloned initial state.
     *
     * @param roomId Room whose state should be reconstructed.
     * @param state Application-defined room state to replace after success.
     * @return Detailed replay result.
     *
     * @throws vix::realtime::Error
     *         When storage, stream validation, replay limits, or state
     *         application fail.
     */
    [[nodiscard]] ReplayResult restore(
        const RoomId &roomId,
        RoomState &state) const;

    /**
     * @brief Restore one room from an explicitly supplied snapshot.
     *
     * Passing no snapshot starts replay at the initial stream position and
     * ignores the configured snapshot store.
     *
     * @param roomId Room whose state should be reconstructed.
     * @param state Application-defined room state to replace after success.
     * @param snapshot Explicit replay starting snapshot.
     * @return Detailed replay result.
     *
     * @throws vix::realtime::Error
     *         When the snapshot, stream, replay limits, or state are invalid.
     */
    [[nodiscard]] ReplayResult restore_from(
        const RoomId &roomId,
        RoomState &state,
        std::optional<RoomSnapshot> snapshot) const;

    /**
     * @brief Return the authoritative event store.
     *
     * @return Event store.
     */
    [[nodiscard]] const EventStorePtr &
    event_store() const noexcept;

    /**
     * @brief Return the optional snapshot store.
     *
     * @return Snapshot store, or null.
     */
    [[nodiscard]] const SnapshotStorePtr &
    snapshot_store() const noexcept;

    /**
     * @brief Return the configured replay limits.
     *
     * @return Replay options.
     */
    [[nodiscard]] const ReplayOptions &
    options() const noexcept;

  private:
    /**
     * @brief Return a query limit with one overflow detection element.
     */
    [[nodiscard]] std::size_t
    query_limit() const noexcept;

    /**
     * @brief Estimate the serialized size of one persisted event.
     */
    [[nodiscard]] static std::size_t
    serialized_event_size(
        const RoomEvent &event);

    /**
     * @brief Throw when the replay timeout has elapsed.
     */
    void enforce_timeout(
        SteadyTimestamp startedAt) const;

    /** @brief Authoritative persisted room event store. */
    EventStorePtr eventStore_{};

    /** @brief Optional persisted room snapshot store. */
    SnapshotStorePtr snapshotStore_{};

    /** @brief Replay resource limits and behavior. */
    ReplayOptions options_{};
  };

} // namespace vix::realtime::internal

#endif // VIX_REALTIME_INTERNAL_REPLAY_ENGINE_HPP
