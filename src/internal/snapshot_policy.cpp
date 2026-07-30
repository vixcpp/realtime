/**
 *
 * @file snapshot_policy.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime snapshot scheduling policy.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/internal/snapshot_policy.hpp>

#include <cstdint>
#include <limits>

#include <vix/realtime/errors.hpp>

namespace vix::realtime::internal
{
  namespace
  {
    /**
     * @brief Convert a non-negative event distance to std::size_t.
     */
    [[nodiscard]] std::size_t to_size(
        EventIdValue value)
    {
      if (value < 0)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "snapshot event distance cannot be negative"};
      }

      using UnsignedEventId =
          std::make_unsigned_t<EventIdValue>;

      const auto unsignedValue =
          static_cast<UnsignedEventId>(value);

      if (unsignedValue >
          std::numeric_limits<std::size_t>::max())
      {
        return std::numeric_limits<std::size_t>::max();
      }

      return static_cast<std::size_t>(
          unsignedValue);
    }

  } // namespace

  SnapshotPolicy::SnapshotPolicy(
      std::size_t everyEvents,
      std::size_t snapshotsToKeep,
      bool snapshotOnRoomClose)
      : everyEvents_(everyEvents),
        snapshotsToKeep_(snapshotsToKeep),
        snapshotOnRoomClose_(snapshotOnRoomClose)
  {
    if (snapshotsToKeep_ == 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "snapshot retention count must be greater than zero"};
    }
  }

  SnapshotPolicy SnapshotPolicy::from_config(
      const Config &config)
  {
    config.validate();

    return SnapshotPolicy{
        config.snapshotEveryEvents,
        config.snapshotsToKeep,
        config.snapshotOnRoomClose};
  }

  SnapshotDecision SnapshotPolicy::evaluate(
      RoomVersion roomVersion,
      EventId lastEventId,
      const RoomSnapshot *latestSnapshot,
      bool roomClosing,
      bool explicitRequest) const
  {
    if (roomVersion.is_initial() &&
        !lastEventId.empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "initial room state cannot reference a persisted event"};
    }

    if (!roomVersion.is_initial() &&
        lastEventId.empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "versioned room state requires a last event identifier"};
    }

    EventId snapshotEventId{};
    RoomVersion snapshotVersion{};

    if (latestSnapshot != nullptr)
    {
      latestSnapshot->validate();

      snapshotEventId =
          latestSnapshot->last_event_id();

      snapshotVersion =
          latestSnapshot->room_version();

      if (snapshotVersion > roomVersion)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "latest snapshot version is ahead of the current room version"};
      }

      if (snapshotEventId > lastEventId)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "latest snapshot event identifier is ahead of the current room stream"};
      }

      if (snapshotVersion == roomVersion &&
          snapshotEventId != lastEventId)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "snapshot and room positions disagree at the same room version"};
      }
    }

    const EventIdValue eventDistance =
        lastEventId.value() -
        snapshotEventId.value();

    SnapshotDecision decision{
        false,
        SnapshotReason::None,
        to_size(eventDistance),
        snapshotsToKeep_};

    const bool stateAdvanced =
        roomVersion > snapshotVersion ||
        lastEventId > snapshotEventId;

    if (!stateAdvanced)
    {
      return decision;
    }

    if (explicitRequest)
    {
      decision.required = true;
      decision.reason = SnapshotReason::Explicit;
      return decision;
    }

    if (roomClosing &&
        snapshotOnRoomClose_)
    {
      decision.required = true;
      decision.reason = SnapshotReason::RoomClose;
      return decision;
    }

    if (everyEvents_ != 0 &&
        decision.eventsSinceSnapshot >= everyEvents_)
    {
      decision.required = true;
      decision.reason = SnapshotReason::EventInterval;
    }

    return decision;
  }

  std::size_t
  SnapshotPolicy::every_events() const noexcept
  {
    return everyEvents_;
  }

  std::size_t
  SnapshotPolicy::snapshots_to_keep() const noexcept
  {
    return snapshotsToKeep_;
  }

  bool SnapshotPolicy::snapshot_on_room_close() const noexcept
  {
    return snapshotOnRoomClose_;
  }

  bool SnapshotPolicy::interval_enabled() const noexcept
  {
    return everyEvents_ != 0;
  }

} // namespace vix::realtime::internal
