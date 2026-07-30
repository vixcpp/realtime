/**
 *
 * @file room.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime authoritative room runtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/room.hpp>

#include <limits>
#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  namespace
  {
    /**
     * @brief Return a replay limit with one additional overflow element.
     */
    [[nodiscard]] std::size_t replay_query_limit(
        std::size_t configuredLimit) noexcept
    {
      if (configuredLimit ==
          std::numeric_limits<std::size_t>::max())
      {
        return configuredLimit;
      }

      return configuredLimit + 1;
    }

    /**
     * @brief Return whether a command result permits a lifecycle transition.
     */
    [[nodiscard]] bool lifecycle_accepted(
        const CommandResult &result) noexcept
    {
      return !result.is_rejected();
    }

  } // namespace

  Room::Room(
      RoomId roomId,
      std::string roomType,
      RoomComponents components,
      EventStorePtr eventStore,
      SnapshotStorePtr snapshotStore,
      Config config,
      std::shared_ptr<internal::EventDispatcher> eventDispatcher,
      std::optional<NodeId> ownerNodeId,
      JsonObject metadata)
      : roomId_(std::move(roomId)),
        roomType_(std::move(roomType)),
        state_(std::move(components.state)),
        handler_(std::move(components.handler)),
        eventStore_(std::move(eventStore)),
        snapshotStore_(std::move(snapshotStore)),
        config_(std::move(config)),
        commandQueue_(config_.maxPendingCommandsPerRoom),
        snapshotPolicy_(
            config_.snapshotEveryEvents,
            config_.snapshotsToKeep,
            config_.snapshotOnRoomClose),
        eventDispatcher_(std::move(eventDispatcher)),
        status_(RoomStatus::Created),
        roomVersion_(),
        lastEventId_(),
        sessions_(),
        ownerNodeId_(std::move(ownerNodeId)),
        lastActivityAt_(SystemClock::now()),
        metadata_(std::move(metadata))
  {
    if (roomId_.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room requires an identifier"};
    }

    if (!RoomFactory::is_valid_type(roomType_))
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room type is invalid"};
    }

    if (!state_)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "room requires an authoritative state"};
    }

    if (!handler_)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "room requires a command handler"};
    }

    if (!eventStore_)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "room requires an event store"};
    }

    if (state_->schema_version() == 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room state schema version cannot be zero"};
    }

    if (config_.maxSessionsPerRoom == 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room session capacity must be greater than zero"};
    }

    if (config_.maxReplayEvents == 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room replay limit must be greater than zero"};
    }

    if (ownerNodeId_ &&
        ownerNodeId_->empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room owner node identifier cannot be empty"};
    }
  }

  Room::~Room()
  {
    commandQueue_.close();
  }

  CommandResult Room::open()
  {
    std::vector<RoomEvent> committedEvents;
    std::vector<SessionId> recipients;
    std::shared_ptr<internal::EventDispatcher> dispatcher;
    CommandResult result;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      if (status_ == RoomStatus::Open)
      {
        return CommandResult::ignored(
            "room is already open");
      }

      if (status_ != RoomStatus::Created)
      {
        throw Error{
            ErrorCode::RoomNotReady,
            "room cannot be opened from its current state"};
      }

      status_ = RoomStatus::Opening;

      try
      {
        restore_locked();

        const Timestamp now =
            SystemClock::now();

        RoomContext context =
            make_context_locked(
                std::nullopt,
                {},
                {},
                now);

        result =
            handler_->on_open(
                *state_,
                context);

        result.validate();

        if (result.is_rejected())
        {
          status_ = RoomStatus::Failed;

          throw Error{
              result.error_code().value_or(
                  ErrorCode::CommandRejected),
              result.message().empty()
                  ? "room open callback rejected the room"
                  : result.message()};
        }

        committedEvents =
            commit_result_locked(
                result,
                context);

        status_ = RoomStatus::Open;
        touch_locked(now);

        recipients = sessions_locked();
        dispatcher = eventDispatcher_;
      }
      catch (...)
      {
        status_ = RoomStatus::Failed;
        commandQueue_.close();
        throw;
      }
    }

    dispatch_events(
        committedEvents,
        recipients,
        dispatcher);

    return result;
  }

  CommandResult Room::close()
  {
    std::vector<RoomEvent> committedEvents;
    std::vector<SessionId> recipients;
    std::shared_ptr<internal::EventDispatcher> dispatcher;
    CommandResult result;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      if (status_ == RoomStatus::Closed)
      {
        return CommandResult::ignored(
            "room is already closed");
      }

      if (status_ != RoomStatus::Open)
      {
        throw Error{
            ErrorCode::RoomNotReady,
            "room cannot be closed from its current state"};
      }

      status_ = RoomStatus::Closing;

      const Timestamp now =
          SystemClock::now();

      try
      {
        RoomContext context =
            make_context_locked(
                std::nullopt,
                {},
                {},
                now);

        result =
            handler_->on_close(
                *state_,
                context);

        result.validate();

        if (result.is_rejected())
        {
          status_ = RoomStatus::Open;
          return result;
        }

        committedEvents =
            commit_result_locked(
                result,
                context);

        recipients = sessions_locked();
        dispatcher = eventDispatcher_;

        if (snapshotStore_)
        {
          static_cast<void>(
              save_snapshot_locked(
                  false,
                  true));
        }

        sessions_.clear();
        status_ = RoomStatus::Closed;
        touch_locked(now);
        commandQueue_.close();
      }
      catch (...)
      {
        status_ = RoomStatus::Failed;
        commandQueue_.close();
        throw;
      }
    }

    dispatch_events(
        committedEvents,
        recipients,
        dispatcher);

    return result;
  }

  internal::CommandQueueStatus Room::enqueue(
      RoomCommand command)
  {
    command.validate();

    if (command.room_id() != roomId_)
    {
      throw Error{
          ErrorCode::InvalidCommand,
          "command belongs to a different room"};
    }

    {
      std::lock_guard<std::mutex> lock{mutex_};

      if (status_ == RoomStatus::Closed ||
          status_ == RoomStatus::Closing)
      {
        return internal::CommandQueueStatus::Closed;
      }

      if (status_ != RoomStatus::Open)
      {
        throw Error{
            ErrorCode::RoomNotReady,
            "room is not ready to accept commands"};
      }
    }

    return commandQueue_.try_push(
        std::move(command));
  }

  std::optional<CommandResult>
  Room::process_next()
  {
    auto queued = commandQueue_.try_pop();

    if (queued.status ==
            internal::CommandQueueStatus::Empty ||
        queued.status ==
            internal::CommandQueueStatus::Closed)
    {
      return std::nullopt;
    }

    if (!queued ||
        !queued.command)
    {
      return std::nullopt;
    }

    return execute(*queued.command);
  }

  CommandResult Room::execute(
      const RoomCommand &command)
  {
    command.validate();

    std::vector<RoomEvent> committedEvents;
    std::vector<SessionId> recipients;
    std::shared_ptr<internal::EventDispatcher> dispatcher;
    CommandResult result;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      if (status_ == RoomStatus::Closed ||
          status_ == RoomStatus::Closing)
      {
        throw Error{
            ErrorCode::RoomClosed,
            "room is closed"};
      }

      if (status_ != RoomStatus::Open)
      {
        throw Error{
            ErrorCode::RoomNotReady,
            "room is not ready to process commands"};
      }

      if (command.room_id() != roomId_)
      {
        throw Error{
            ErrorCode::InvalidCommand,
            "command belongs to a different room"};
      }

      if (!sessions_.contains(
              command.session_id()))
      {
        throw Error{
            ErrorCode::MembershipNotFound,
            "command session is not a member of the room"};
      }

      if (command.expected_version() &&
          *command.expected_version() != roomVersion_)
      {
        return CommandResult::rejected(
            ErrorCode::CommandRejected,
            "room version does not match the expected version");
      }

      const Timestamp now =
          SystemClock::now();

      RoomContext context =
          RoomContext::from_command(
              command,
              roomVersion_,
              lastEventId_,
              ownerNodeId_,
              now,
              metadata_);

      result =
          handler_->handle_command(
              command,
              *state_,
              context);

      result.validate();

      committedEvents =
          commit_result_locked(
              result,
              context);

      try_automatic_snapshot_locked();
      touch_locked(now);

      recipients = sessions_locked();
      dispatcher = eventDispatcher_;
    }

    dispatch_events(
        committedEvents,
        recipients,
        dispatcher);

    return result;
  }

  CommandResult Room::join(
      const SessionId &sessionId)
  {
    if (sessionId.empty())
    {
      throw Error{
          ErrorCode::SessionNotFound,
          "room join requires a session identifier"};
    }

    std::vector<RoomEvent> committedEvents;
    std::vector<SessionId> recipients;
    std::shared_ptr<internal::EventDispatcher> dispatcher;
    CommandResult result;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      if (status_ != RoomStatus::Open)
      {
        throw Error{
            status_ == RoomStatus::Closed
                ? ErrorCode::RoomClosed
                : ErrorCode::RoomNotReady,
            "room is not available for session joins"};
      }

      if (sessions_.contains(sessionId))
      {
        throw Error{
            ErrorCode::AlreadyJoined,
            "session already belongs to the room"};
      }

      if (sessions_.size() >=
          config_.maxSessionsPerRoom)
      {
        throw Error{
            ErrorCode::RoomFull,
            "room reached its session capacity"};
      }

      const Timestamp now =
          SystemClock::now();

      RoomContext context =
          make_context_locked(
              sessionId,
              {},
              {},
              now);

      result =
          handler_->on_join(
              sessionId,
              *state_,
              context);

      result.validate();

      if (!lifecycle_accepted(result))
      {
        return result;
      }

      sessions_.insert(sessionId);

      try
      {
        committedEvents =
            commit_result_locked(
                result,
                context);
      }
      catch (...)
      {
        sessions_.erase(sessionId);
        throw;
      }

      try_automatic_snapshot_locked();
      touch_locked(now);

      recipients = sessions_locked();
      dispatcher = eventDispatcher_;
    }

    dispatch_events(
        committedEvents,
        recipients,
        dispatcher);

    return result;
  }

  CommandResult Room::leave(
      const SessionId &sessionId)
  {
    if (sessionId.empty())
    {
      throw Error{
          ErrorCode::SessionNotFound,
          "room leave requires a session identifier"};
    }

    std::vector<RoomEvent> committedEvents;
    std::vector<SessionId> recipients;
    std::shared_ptr<internal::EventDispatcher> dispatcher;
    CommandResult result;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      if (status_ != RoomStatus::Open)
      {
        throw Error{
            status_ == RoomStatus::Closed
                ? ErrorCode::RoomClosed
                : ErrorCode::RoomNotReady,
            "room is not available for session leaves"};
      }

      if (!sessions_.contains(sessionId))
      {
        throw Error{
            ErrorCode::MembershipNotFound,
            "session does not belong to the room"};
      }

      const Timestamp now =
          SystemClock::now();

      RoomContext context =
          make_context_locked(
              sessionId,
              {},
              {},
              now);

      result =
          handler_->on_leave(
              sessionId,
              *state_,
              context);

      result.validate();

      if (!lifecycle_accepted(result))
      {
        return result;
      }

      committedEvents =
          commit_result_locked(
              result,
              context);

      recipients = sessions_locked();
      dispatcher = eventDispatcher_;

      sessions_.erase(sessionId);

      try_automatic_snapshot_locked();
      touch_locked(now);
    }

    dispatch_events(
        committedEvents,
        recipients,
        dispatcher);

    return result;
  }

  std::optional<RoomSnapshot>
  Room::snapshot(bool force)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (status_ != RoomStatus::Open &&
        status_ != RoomStatus::Closing)
    {
      throw Error{
          ErrorCode::RoomNotReady,
          "room is not available for snapshot creation"};
    }

    return save_snapshot_locked(
        force,
        status_ == RoomStatus::Closing);
  }

  const RoomId &Room::id() const noexcept
  {
    return roomId_;
  }

  const std::string &Room::type() const noexcept
  {
    return roomType_;
  }

  RoomStatus Room::status() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return status_;
  }

  bool Room::is_open() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return status_ == RoomStatus::Open;
  }

  bool Room::failed() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return status_ == RoomStatus::Failed;
  }

  RoomVersion Room::version() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return roomVersion_;
  }

  EventId Room::last_event_id() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return lastEventId_;
  }

  JsonObject Room::serialize_state() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return state_->serialize();
  }

  bool Room::has_session(
      const SessionId &sessionId) const
  {
    if (sessionId.empty())
    {
      return false;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    return sessions_.contains(sessionId);
  }

  std::vector<SessionId> Room::sessions() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return sessions_locked();
  }

  std::size_t Room::session_count() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return sessions_.size();
  }

  std::size_t Room::pending_command_count() const
  {
    return commandQueue_.size();
  }

  Timestamp Room::last_activity_at() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return lastActivityAt_;
  }

  std::optional<NodeId>
  Room::owner_node_id() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return ownerNodeId_;
  }

  void Room::set_owner_node_id(NodeId nodeId)
  {
    if (nodeId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room owner node identifier cannot be empty"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    ownerNodeId_ = std::move(nodeId);
  }

  void Room::clear_owner_node_id()
  {
    std::lock_guard<std::mutex> lock{mutex_};

    ownerNodeId_.reset();
  }

  void Room::set_event_dispatcher(
      std::shared_ptr<internal::EventDispatcher> dispatcher)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    eventDispatcher_ = std::move(dispatcher);
  }

  JsonObject Room::metadata() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return metadata_;
  }

  void Room::set_metadata(JsonObject value)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    metadata_ = std::move(value);
  }

  RoomContext Room::make_context_locked(
      std::optional<SessionId> sessionId,
      RequestId requestId,
      CorrelationId correlationId,
      Timestamp now) const
  {
    return RoomContext{
        roomId_,
        roomVersion_,
        lastEventId_,
        std::move(sessionId),
        std::move(requestId),
        std::move(correlationId),
        ownerNodeId_,
        now,
        metadata_};
  }

  std::vector<RoomEvent>
  Room::commit_result_locked(
      CommandResult &result,
      const RoomContext &context)
  {
    result.validate();

    if (!result.is_accepted() ||
        result.events().empty())
    {
      return {};
    }

    std::vector<RoomEvent> events =
        result.events();

    RoomVersion nextVersion =
        roomVersion_;

    for (auto &event : events)
    {
      if (event.room_id() != roomId_)
      {
        throw Error{
            ErrorCode::InvalidCommand,
            "command result contains an event for another room"};
      }

      if (!event.event_id().empty())
      {
        throw Error{
            ErrorCode::InvalidCommand,
            "command result event already has a persistent identifier"};
      }

      nextVersion.increment();
      event.set_room_version(nextVersion);

      if (!event.source_session() &&
          context.session_id())
      {
        event.set_source_session(
            *context.session_id());
      }

      if (event.request_id().empty() &&
          !context.request_id().empty())
      {
        event.set_request_id(
            context.request_id());
      }

      if (event.correlation_id().empty() &&
          !context.correlation_id().empty())
      {
        event.set_correlation_id(
            context.correlation_id());
      }

      event.set_schema_version(
          state_->schema_version());

      event.set_created_at(
          context.now());

      event.validate();
    }

    std::vector<RoomEvent> persisted =
        eventStore_->append_batch(
            std::move(events));

    if (persisted.size() !=
        result.event_count())
    {
      status_ = RoomStatus::Failed;

      throw Error{
          ErrorCode::EventStoreFailure,
          "event store returned an incomplete persisted batch"};
    }

    try
    {
      for (const auto &event : persisted)
      {
        state_->apply(event);
        roomVersion_ = event.room_version();
        lastEventId_ = event.event_id();
      }
    }
    catch (const Error &error)
    {
      status_ = RoomStatus::Failed;

      throw Error{
          ErrorCode::EventApplyFailure,
          std::string{
              "failed to apply persisted room event: "} +
              error.what()};
    }
    catch (const std::exception &error)
    {
      status_ = RoomStatus::Failed;

      throw Error{
          ErrorCode::EventApplyFailure,
          std::string{
              "failed to apply persisted room event: "} +
              error.what()};
    }
    catch (...)
    {
      status_ = RoomStatus::Failed;

      throw Error{
          ErrorCode::EventApplyFailure,
          "failed to apply persisted room event"};
    }

    result.events() = persisted;
    return persisted;
  }

  void Room::restore_locked()
  {
    roomVersion_ = RoomVersion{};
    lastEventId_ = EventId{};

    if (!config_.restoreRoomsOnOpen)
    {
      if (!eventStore_->latest_event_id(roomId_).empty())
      {
        throw Error{
            ErrorCode::ReplayUnavailable,
            "room restoration is disabled but persisted events exist"};
      }

      return;
    }

    if (snapshotStore_)
    {
      const auto latestSnapshot =
          snapshotStore_->load_latest(roomId_);

      if (latestSnapshot)
      {
        latestSnapshot->validate();

        if (latestSnapshot->room_id() != roomId_)
        {
          throw Error{
              ErrorCode::CorruptedState,
              "loaded snapshot belongs to another room"};
        }

        state_->restore(
            latestSnapshot->state(),
            latestSnapshot->schema_version());

        roomVersion_ =
            latestSnapshot->room_version();

        lastEventId_ =
            latestSnapshot->last_event_id();
      }
    }

    const std::size_t queryLimit =
        replay_query_limit(
            config_.maxReplayEvents);

    const std::vector<RoomEvent> replayEvents =
        eventStore_->load_after(
            roomId_,
            lastEventId_,
            queryLimit);

    if (replayEvents.size() >
        config_.maxReplayEvents)
    {
      throw Error{
          ErrorCode::ReplayLimitExceeded,
          "room replay exceeds the configured event limit"};
    }

    for (const auto &event : replayEvents)
    {
      apply_replay_event_locked(event);
    }
  }

  void Room::apply_replay_event_locked(
      const RoomEvent &event)
  {
    event.validate();

    if (event.room_id() != roomId_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "replayed event belongs to another room"};
    }

    const RoomVersion expectedVersion =
        roomVersion_.next();

    if (event.room_version() !=
        expectedVersion)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "replayed room event version is not contiguous"};
    }

    const EventId expectedEventId =
        lastEventId_.next();

    if (event.event_id() !=
        expectedEventId)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "replayed event identifier is not contiguous"};
    }

    try
    {
      state_->apply(event);
    }
    catch (const Error &error)
    {
      throw Error{
          ErrorCode::EventApplyFailure,
          std::string{
              "failed to replay room event: "} +
              error.what()};
    }
    catch (const std::exception &error)
    {
      throw Error{
          ErrorCode::EventApplyFailure,
          std::string{
              "failed to replay room event: "} +
              error.what()};
    }
    catch (...)
    {
      throw Error{
          ErrorCode::EventApplyFailure,
          "failed to replay room event"};
    }

    roomVersion_ =
        event.room_version();

    lastEventId_ =
        event.event_id();
  }

  std::optional<RoomSnapshot>
  Room::save_snapshot_locked(
      bool force,
      bool roomClosing)
  {
    if (!snapshotStore_)
    {
      if (force)
      {
        throw Error{
            ErrorCode::MissingDependency,
            "room snapshot store is not configured"};
      }

      return std::nullopt;
    }

    std::optional<RoomSnapshot> latest =
        snapshotStore_->load_latest(roomId_);

    if (!force)
    {
      const internal::SnapshotDecision decision =
          snapshotPolicy_.evaluate(
              roomVersion_,
              lastEventId_,
              latest ? &*latest : nullptr,
              roomClosing,
              false);

      if (!decision)
      {
        return std::nullopt;
      }
    }

    RoomSnapshot snapshot{
        roomId_,
        roomVersion_,
        lastEventId_,
        state_->serialize(),
        state_->schema_version()};

    snapshot
        .set_created_at(SystemClock::now())
        .set_metadata(metadata_);

    RoomSnapshot persisted =
        snapshotStore_->save(
            std::move(snapshot));

    snapshotStore_->prune(
        roomId_,
        snapshotPolicy_.snapshots_to_keep());

    return persisted;
  }

  void Room::try_automatic_snapshot_locked() noexcept
  {
    if (!snapshotStore_)
    {
      return;
    }

    try
    {
      static_cast<void>(
          save_snapshot_locked(
              false,
              false));
    }
    catch (...)
    {
      /*
       * Events and state are already authoritative at this point. Automatic
       * snapshot failure must not report the committed command as rejected.
       */
    }
  }

  void Room::dispatch_events(
      const std::vector<RoomEvent> &events,
      const std::vector<SessionId> &roomSessions,
      const std::shared_ptr<internal::EventDispatcher> &dispatcher) const noexcept
  {
    if (!dispatcher)
    {
      return;
    }

    for (const auto &event : events)
    {
      try
      {
        static_cast<void>(
            dispatcher->dispatch(
                event,
                roomSessions));
      }
      catch (...)
      {
        /*
         * Event persistence and state application already succeeded. Delivery
         * failures are isolated from authoritative command completion.
         */
      }
    }
  }

  std::vector<SessionId>
  Room::sessions_locked() const
  {
    return {
        sessions_.begin(),
        sessions_.end()};
  }

  void Room::touch_locked(Timestamp now) noexcept
  {
    lastActivityAt_ = now;
  }

} // namespace vix::realtime
