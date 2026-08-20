/**
 *
 * @file replay_engine.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime snapshot restoration and event replay.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/internal/replay_engine.hpp>

#include <exception>
#include <limits>
#include <string>
#include <utility>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/protocol.hpp>

namespace vix::realtime::internal
{
  void ReplayOptions::validate() const
  {
    if (maxEvents == 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "replay event limit must be greater than zero"};
    }

    if (maxBytes == 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "replay byte limit must be greater than zero"};
    }

    if (timeout.count() <= 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "replay timeout must be greater than zero"};
    }
  }

  ReplayEngine::ReplayEngine(
      EventStorePtr eventStore,
      SnapshotStorePtr snapshotStore,
      ReplayOptions options)
      : eventStore_(std::move(eventStore)),
        snapshotStore_(std::move(snapshotStore)),
        options_(std::move(options))
  {
    if (!eventStore_)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "replay engine requires an event store"};
    }

    options_.validate();
  }

  ReplayEngine ReplayEngine::from_config(
      const Config &config,
      EventStorePtr eventStore,
      SnapshotStorePtr snapshotStore)
  {
    config.validate();

    ReplayOptions options{
        config.maxReplayEvents,
        config.maxReplayBytes,
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
            config.replayTimeout),
        config.restoreRoomsOnOpen};

    return ReplayEngine{
        std::move(eventStore),
        std::move(snapshotStore),
        std::move(options)};
  }

  ReplayResult ReplayEngine::restore(
      const RoomId &roomId,
      RoomState &state) const
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "room replay requires a room identifier"};
    }

    std::optional<RoomSnapshot> snapshot;

    if (options_.restoreLatestSnapshot &&
        snapshotStore_)
    {
      snapshot =
          snapshotStore_->load_latest(
              roomId);
    }

    return restore_from(
        roomId,
        state,
        std::move(snapshot));
  }

  ReplayResult ReplayEngine::restore_from(
      const RoomId &roomId,
      RoomState &state,
      std::optional<RoomSnapshot> snapshot) const
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "room replay requires a room identifier"};
    }

    options_.validate();

    const SteadyTimestamp startedAt =
        SteadyClock::now();

    RoomStatePtr workingState =
        state.clone();

    if (!workingState)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "room state clone returned a null state"};
    }

    ReplayResult result;

    if (snapshot)
    {
      snapshot->validate();

      if (snapshot->room_id() != roomId)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "replay snapshot belongs to another room"};
      }

      enforce_timeout(startedAt);

      try
      {
        workingState->restore(
            snapshot->state(),
            snapshot->schema_version());
      }
      catch (const Error &error)
      {
        throw Error{
            ErrorCode::CorruptedState,
            std::string{
                "failed to restore room snapshot: "} +
                error.what()};
      }
      catch (const std::exception &error)
      {
        throw Error{
            ErrorCode::CorruptedState,
            std::string{
                "failed to restore room snapshot: "} +
                error.what()};
      }
      catch (...)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "failed to restore room snapshot"};
      }

      result.roomVersion =
          snapshot->room_version();

      result.lastEventId =
          snapshot->last_event_id();

      result.snapshot =
          std::move(snapshot);
    }

    enforce_timeout(startedAt);

    const EventId observedLatestEventId =
        eventStore_->latest_event_id(
            roomId);

    if (observedLatestEventId <
        result.lastEventId)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "snapshot event position is ahead of the event store"};
    }

    std::vector<RoomEvent> replayEvents =
        eventStore_->load_after(
            roomId,
            result.lastEventId,
            query_limit());

    if (replayEvents.size() >
        options_.maxEvents)
    {
      throw Error{
          ErrorCode::ReplayLimitExceeded,
          "room replay exceeds the configured event limit"};
    }

    result.events.reserve(
        replayEvents.size());

    for (const auto &event : replayEvents)
    {
      enforce_timeout(startedAt);

      event.validate();

      if (event.room_id() != roomId)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "replayed event belongs to another room"};
      }

      const RoomVersion expectedRoomVersion =
          result.roomVersion.next();

      if (event.room_version() !=
          expectedRoomVersion)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "replayed room event version is not contiguous"};
      }

      const EventId expectedEventId =
          result.lastEventId.next();

      if (event.event_id() !=
          expectedEventId)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "replayed event identifier is not contiguous"};
      }

      const std::size_t eventBytes =
          serialized_event_size(event);

      if (eventBytes >
              options_.maxBytes ||
          result.replayBytes >
              options_.maxBytes - eventBytes)
      {
        throw Error{
            ErrorCode::ReplayLimitExceeded,
            "room replay exceeds the configured byte limit"};
      }

      try
      {
        workingState->apply(event);
      }
      catch (const Error &error)
      {
        throw Error{
            ErrorCode::EventApplyFailure,
            std::string{
                "failed to apply replayed room event: "} +
                error.what()};
      }
      catch (const std::exception &error)
      {
        throw Error{
            ErrorCode::EventApplyFailure,
            std::string{
                "failed to apply replayed room event: "} +
                error.what()};
      }
      catch (...)
      {
        throw Error{
            ErrorCode::EventApplyFailure,
            "failed to apply replayed room event"};
      }

      result.roomVersion =
          event.room_version();

      result.lastEventId =
          event.event_id();

      result.replayBytes +=
          eventBytes;

      ++result.eventCount;

      result.events.push_back(event);
    }

    enforce_timeout(startedAt);

    if (result.lastEventId <
        observedLatestEventId)
    {
      if (result.eventCount >=
          options_.maxEvents)
      {
        throw Error{
            ErrorCode::ReplayLimitExceeded,
            "room replay exceeds the configured event limit"};
      }

      throw Error{
          ErrorCode::ReplayUnavailable,
          "event store did not return the complete room replay stream"};
    }

    try
    {
      state.restore(
          workingState->serialize(),
          workingState->schema_version());
    }
    catch (const Error &error)
    {
      throw Error{
          ErrorCode::EventApplyFailure,
          std::string{
              "failed to commit reconstructed room state: "} +
              error.what()};
    }
    catch (const std::exception &error)
    {
      throw Error{
          ErrorCode::EventApplyFailure,
          std::string{
              "failed to commit reconstructed room state: "} +
              error.what()};
    }
    catch (...)
    {
      throw Error{
          ErrorCode::EventApplyFailure,
          "failed to commit reconstructed room state"};
    }

    result.elapsed =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
            SteadyClock::now() -
            startedAt);

    return result;
  }

  ReplayResult ReplayEngine::recover(
      const RoomId &roomId,
      EventId cursor,
      SteadyTimestamp startedAt,
      std::optional<RoomVersion> knownRoomVersion,
      bool allowSnapshotFallback,
      bool requireCompleteStream) const
  {
    if (roomId.empty())
    {
      throw Error{ErrorCode::RoomNotFound,
                  "replay recovery requires a room identifier"};
    }

    options_.validate();
    enforce_timeout(startedAt);

    ReplayResult result;
    result.lastEventId = cursor;
    if (knownRoomVersion)
    {
      result.roomVersion = *knownRoomVersion;
    }
    const EventId observedLatestEventId = eventStore_->latest_event_id(roomId);
    std::vector<RoomEvent> events = eventStore_->load_after(
        roomId, cursor, query_limit());

    if (events.size() > options_.maxEvents)
    {
      if (!allowSnapshotFallback || !snapshotStore_)
      {
        throw Error{ErrorCode::ReplayLimitExceeded,
                    "replay exceeds the configured event limit"};
      }

      const auto snapshot = snapshotStore_->load_latest(roomId);
      if (!snapshot || snapshot->last_event_id().empty() ||
          snapshot->last_event_id() <= cursor)
      {
        throw Error{ErrorCode::ReplayLimitExceeded,
                    "replay exceeds its event limit without a usable snapshot"};
      }

      snapshot->validate();
      if (snapshot->room_id() != roomId)
      {
        throw Error{ErrorCode::CorruptedState,
                    "replay snapshot belongs to another room"};
      }

      result.snapshot = *snapshot;
      result.roomVersion = snapshot->room_version();
      result.lastEventId = snapshot->last_event_id();
      events = eventStore_->load_after(roomId, result.lastEventId, query_limit());
      if (events.size() > options_.maxEvents)
      {
        throw Error{ErrorCode::ReplayLimitExceeded,
                    "replay remains above its event limit after snapshot recovery"};
      }
    }

    bool validateVersion = knownRoomVersion.has_value() ||
        result.snapshot.has_value() || cursor.empty();
    result.events.reserve(events.size());
    for (const auto &event : events)
    {
      enforce_timeout(startedAt);
      event.validate();
      if (event.room_id() != roomId || event.event_id() != result.lastEventId.next())
      {
        throw Error{ErrorCode::CorruptedState,
                    "replay event stream is inconsistent"};
      }
      if (validateVersion && event.room_version() != result.roomVersion.next())
      {
        throw Error{ErrorCode::CorruptedState,
                    "replay room version is not contiguous"};
      }

      const std::size_t eventBytes = serialized_event_size(event);
      if (eventBytes > options_.maxBytes ||
          result.replayBytes > options_.maxBytes - eventBytes)
      {
        throw Error{ErrorCode::ReplayLimitExceeded,
                    "replay exceeds the configured byte limit"};
      }

      result.lastEventId = event.event_id();
      result.roomVersion = event.room_version();
      result.replayBytes += eventBytes;
      ++result.eventCount;
      result.events.push_back(event);
      validateVersion = true;
    }

    enforce_timeout(startedAt);
    if (requireCompleteStream &&
        result.lastEventId < observedLatestEventId)
    {
      throw Error{ErrorCode::ReplayUnavailable,
                  "event store did not return the complete replay stream"};
    }

    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        SteadyClock::now() - startedAt);
    return result;
  }

  const EventStorePtr &
  ReplayEngine::event_store() const noexcept
  {
    return eventStore_;
  }

  const SnapshotStorePtr &
  ReplayEngine::snapshot_store() const noexcept
  {
    return snapshotStore_;
  }

  const ReplayOptions &
  ReplayEngine::options() const noexcept
  {
    return options_;
  }

  std::size_t
  ReplayEngine::query_limit() const noexcept
  {
    if (options_.maxEvents ==
        std::numeric_limits<std::size_t>::max())
    {
      return options_.maxEvents;
    }

    return options_.maxEvents + 1;
  }

  std::size_t ReplayEngine::serialized_event_size(
      const RoomEvent &event)
  {
    try
    {
      return protocol::serialize(
                 protocol::from_event(event))
          .size();
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
              "failed to measure replayed room event: "} +
              error.what()};
    }
    catch (...)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "failed to measure replayed room event"};
    }
  }

  void ReplayEngine::enforce_timeout(
      SteadyTimestamp startedAt) const
  {
    const auto elapsed =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
            SteadyClock::now() -
            startedAt);

    if (elapsed >= options_.timeout)
    {
      throw Error{
          ErrorCode::Timeout,
          "room replay exceeded the configured timeout"};
    }
  }

} // namespace vix::realtime::internal
