/**
 *
 * @file room_manager.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime room and session manager.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/room_manager.hpp>

#include <algorithm>
#include <utility>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/local_presence_store.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/internal/event_dispatcher.hpp>

namespace vix::realtime
{
  namespace
  {
    /**
     * @brief Create the default presence store when presence is enabled.
     */
    [[nodiscard]] PresenceStorePtr
    make_default_presence_store(
        const Config &config)
    {
      if (!config.enablePresence)
      {
        return nullptr;
      }

      return std::make_shared<LocalPresenceStore>();
    }

    /**
     * @brief Validate manager-level configuration values.
     */
    void validate_manager_config(
        const Config &config)
    {
      if (config.maxActiveRooms == 0)
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "realtime manager active room limit must be greater than zero"};
      }

      if (config.maxSessions == 0)
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "realtime manager session limit must be greater than zero"};
      }

      if (config.maxSessionsPerRoom == 0)
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "realtime room session limit must be greater than zero"};
      }

      if (config.maxRoomsPerSession == 0)
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "realtime session room limit must be greater than zero"};
      }

      if (config.maxPendingCommandsPerRoom == 0)
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "realtime room command queue limit must be greater than zero"};
      }

      if (config.snapshotsToKeep == 0)
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "realtime snapshot retention must be greater than zero"};
      }
    }

  } // namespace

  RoomManager::RoomManager(
      NodeId nodeId,
      Config config)
      : RoomManager(
            std::move(nodeId),
            config,
            std::make_shared<MemoryEventStore>(),
            std::make_shared<MemorySnapshotStore>(),
            make_default_presence_store(config),
            std::make_shared<RoomDirectory>())
  {
  }

  RoomManager::RoomManager(
      NodeId nodeId,
      Config config,
      EventStorePtr eventStore,
      SnapshotStorePtr snapshotStore,
      PresenceStorePtr presenceStore,
      std::shared_ptr<RoomDirectory> roomDirectory)
      : nodeId_(std::move(nodeId)),
        config_(std::move(config)),
        eventStore_(std::move(eventStore)),
        snapshotStore_(std::move(snapshotStore)),
        presenceStore_(std::move(presenceStore)),
        roomDirectory_(std::move(roomDirectory)),
        eventDispatcher_(
            std::make_shared<internal::EventDispatcher>()),
        factories_(),
        rooms_(),
        sessions_()
  {
    if (nodeId_.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "realtime room manager requires a node identifier"};
    }

    validate_manager_config(config_);

    if (!eventStore_)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "realtime room manager requires an event store"};
    }

    if (!roomDirectory_)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "realtime room manager requires a room directory"};
    }

    if (config_.enablePresence &&
        !presenceStore_)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "realtime presence is enabled but no presence store is configured"};
    }

    eventDispatcher_->set_delivery_handler(
        [this](
            const SessionId &sessionId,
            const RoomEvent &event)
        {
          deliver_event(sessionId, event);
        });
  }

  RoomManager::~RoomManager()
  {
    if (eventDispatcher_)
    {
      eventDispatcher_->clear_delivery_handler();
      eventDispatcher_->clear_error_handler();
    }

    std::vector<RoomPtr> rooms;
    std::vector<SessionPtr> sessions;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      rooms.reserve(rooms_.size());
      sessions.reserve(sessions_.size());

      for (const auto &[roomId, room] : rooms_)
      {
        static_cast<void>(roomId);
        rooms.push_back(room);
      }

      for (const auto &[sessionId, session] : sessions_)
      {
        static_cast<void>(sessionId);
        sessions.push_back(session);
      }

      rooms_.clear();
      sessions_.clear();
      factories_.clear();
      directFactories_.clear();
    }

    for (const auto &room : rooms)
    {
      if (!room)
      {
        continue;
      }

      try
      {
        if (room->is_open())
        {
          static_cast<void>(room->close());
        }
      }
      catch (...)
      {
      }

      try
      {
        roomDirectory_->clear_room(room->id());
      }
      catch (...)
      {
      }

      if (presenceStore_)
      {
        try
        {
          static_cast<void>(
              presenceStore_->clear_room(
                  room->id()));
        }
        catch (...)
        {
        }
      }
    }

    for (const auto &session : sessions)
    {
      if (!session)
      {
        continue;
      }

      ConnectionPtr connection;

      try
      {
        connection =
            session->close(
                SystemClock::now());
      }
      catch (...)
      {
      }

      if (connection)
      {
        try
        {
          connection->close(
              ErrorCode::Cancelled,
              "realtime manager shutdown");
        }
        catch (...)
        {
        }
      }

      if (presenceStore_)
      {
        try
        {
          static_cast<void>(
              presenceStore_->clear_session(
                  session->id()));
        }
        catch (...)
        {
        }
      }
    }
  }

  bool RoomManager::register_factory(
      RoomFactoryPtr factory,
      bool replace)
  {
    if (!factory)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "cannot register a null room factory"};
    }

    const std::string roomType{
        factory->room_type()};

    if (!RoomFactory::is_valid_type(roomType))
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room factory type is invalid"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        factories_.find(roomType);

    const auto directIterator =
        directFactories_.find(roomType);

    if (iterator != factories_.end() ||
        directIterator != directFactories_.end())
    {
      if (!replace)
      {
        return false;
      }

      directFactories_.erase(roomType);
      factories_.insert_or_assign(
          roomType,
          std::move(factory));

      return true;
    }

    factories_.emplace(
        roomType,
        std::move(factory));

    return true;
  }

  bool RoomManager::unregister_factory(
      std::string_view roomType)
  {
    if (!RoomFactory::is_valid_type(roomType))
    {
      return false;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const std::string key{roomType};

    const bool removedFactory =
        factories_.erase(key) != 0;

    const bool removedDirect =
        directFactories_.erase(key) != 0;

    return removedFactory || removedDirect;
  }

  RoomFactoryPtr RoomManager::find_factory(
      std::string_view roomType) const
  {
    if (!RoomFactory::is_valid_type(roomType))
    {
      return nullptr;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        factories_.find(
            std::string{roomType});

    if (iterator == factories_.end())
    {
      return nullptr;
    }

    return iterator->second;
  }

  bool RoomManager::has_factory(
      std::string_view roomType) const
  {
    if (!RoomFactory::is_valid_type(roomType))
    {
      return false;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const std::string key{roomType};

    return factories_.contains(key) ||
           directFactories_.contains(key);
  }

  std::vector<std::string>
  RoomManager::factory_types() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    std::vector<std::string> result;
    result.reserve(
        factories_.size() +
        directFactories_.size());

    for (const auto &[roomType, factory] : factories_)
    {
      static_cast<void>(factory);
      result.push_back(roomType);
    }

    for (const auto &[roomType, factory] : directFactories_)
    {
      static_cast<void>(factory);
      result.push_back(roomType);
    }

    std::sort(
        result.begin(),
        result.end());

    return result;
  }

  RoomPtr RoomManager::open_room(
      RoomId roomId,
      std::string_view roomType,
      JsonObject metadata)
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room opening requires a room identifier"};
    }

    if (!RoomFactory::is_valid_type(roomType))
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room opening requires a valid room type"};
    }

    RoomFactoryPtr factory;
    std::function<RoomPtr(const RoomId &, const Config &)> directFactory;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      const auto roomIterator =
          rooms_.find(roomId);

      if (roomIterator != rooms_.end())
      {
        const RoomPtr &existing =
            roomIterator->second;

        if (existing->type() != roomType &&
            existing->type() != "default")
        {
          throw Error{
              ErrorCode::RoomAlreadyExists,
              "room already exists with another type"};
        }

        if (existing->is_open())
        {
          return existing;
        }

        throw Error{
            ErrorCode::RoomNotReady,
            "room already exists but is not open"};
      }

      if (rooms_.size() >=
          config_.maxActiveRooms)
      {
        throw Error{
            ErrorCode::RoomLimitReached,
            "realtime manager reached its active room limit"};
      }

      const auto factoryIterator =
          factories_.find(
              std::string{roomType});

      const auto directFactoryIterator =
          directFactories_.find(
              std::string{roomType});

      if (factoryIterator ==
              factories_.end() &&
          directFactoryIterator ==
              directFactories_.end())
      {
        throw Error{
            ErrorCode::MissingDependency,
            "no factory is registered for the requested room type"};
      }

      if (factoryIterator != factories_.end())
      {
        factory = factoryIterator->second;
      }
      else
      {
        directFactory = directFactoryIterator->second;
      }
    }

    RoomOwner owner =
        roomDirectory_->acquire(
            roomId,
            nodeId_,
            std::nullopt,
            SystemClock::now(),
            metadata);

    RoomPtr room;

    try
    {
      if (factory)
      {
        RoomComponents components =
            factory->create(roomId);

        room = std::make_shared<Room>(
            roomId,
            std::string{roomType},
            std::move(components),
            eventStore_,
            snapshotStore_,
            config_,
            nodeId_,
            std::move(metadata));
      }
      else
      {
        room =
            directFactory(
                roomId,
                config_);
      }

      room->set_event_dispatcher(eventDispatcher_);

      {
        std::lock_guard<std::mutex> lock{mutex_};

        if (rooms_.contains(roomId))
        {
          throw Error{
              ErrorCode::RoomAlreadyExists,
              "room was opened concurrently"};
        }

        rooms_.emplace(
            roomId,
            room);
      }

      static_cast<void>(room->open());
      return room;
    }
    catch (...)
    {
      if (room)
      {
        erase_room_if_same(roomId, room);
      }

      try
      {
        if (roomDirectory_->matches(
                roomId,
                nodeId_,
                owner.generation()))
        {
          static_cast<void>(
              roomDirectory_->release(
                  roomId,
                  nodeId_,
                  owner.generation()));
        }
        else
        {
          roomDirectory_->clear_room(roomId);
        }
      }
      catch (...)
      {
        roomDirectory_->clear_room(roomId);
      }

      throw;
    }
  }

  CommandResult RoomManager::close_room(
      const RoomId &roomId,
      bool remove)
  {
    RoomPtr room =
        require_room(roomId);

    const std::vector<SessionId> members =
        room->sessions();

    CommandResult result =
        room->close();

    if (result.is_rejected())
    {
      return result;
    }

    for (const auto &sessionId : members)
    {
      SessionPtr session =
          find_session(sessionId);

      if (session)
      {
        static_cast<void>(
            session->leave_room(roomId));
      }
    }

    if (presenceStore_)
    {
      static_cast<void>(
          presenceStore_->clear_room(roomId));
    }

    const auto owner =
        roomDirectory_->inspect(roomId);

    if (owner &&
        owner->matches(
            nodeId_,
            owner->generation()))
    {
      try
      {
        static_cast<void>(
            roomDirectory_->release(
                roomId,
                nodeId_,
                owner->generation()));
      }
      catch (...)
      {
        roomDirectory_->clear_room(roomId);
      }
    }

    if (remove)
    {
      erase_room_if_same(roomId, room);
    }

    return result;
  }

  RoomPtr RoomManager::find_room(
      const RoomId &roomId) const
  {
    if (roomId.empty())
    {
      return nullptr;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        rooms_.find(roomId);

    if (iterator == rooms_.end())
    {
      return nullptr;
    }

    return iterator->second;
  }

  RoomPtr RoomManager::require_room(
      const RoomId &roomId) const
  {
    RoomPtr room =
        find_room(roomId);

    if (!room)
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "realtime room was not found"};
    }

    return room;
  }

  bool RoomManager::has_room(
      const RoomId &roomId) const
  {
    return find_room(roomId) != nullptr;
  }

  std::vector<RoomId>
  RoomManager::room_ids() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    std::vector<RoomId> result;
    result.reserve(rooms_.size());

    for (const auto &[roomId, room] : rooms_)
    {
      static_cast<void>(room);
      result.push_back(roomId);
    }

    std::sort(
        result.begin(),
        result.end());

    return result;
  }

  std::size_t RoomManager::room_count() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return rooms_.size();
  }

  SessionPtr RoomManager::create_session(
      SessionId sessionId,
      Identity identity,
      ResumeToken resumeToken,
      JsonObject metadata,
      Timestamp now)
  {
    if (sessionId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "session creation requires an identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    if (sessions_.contains(sessionId))
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "logical session already exists"};
    }

    if (sessions_.size() >=
        config_.maxSessions)
    {
      throw Error{
          ErrorCode::CommandRejected,
          "realtime manager reached its logical session limit"};
    }

    SessionPtr session =
        std::make_shared<Session>(
            sessionId,
            std::move(identity),
            std::move(resumeToken),
            now,
            std::move(metadata));

    sessions_.emplace(
        sessionId,
        session);

    return session;
  }

  SessionPtr RoomManager::find_session(
      const SessionId &sessionId) const
  {
    if (sessionId.empty())
    {
      return nullptr;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        sessions_.find(sessionId);

    if (iterator == sessions_.end())
    {
      return nullptr;
    }

    return iterator->second;
  }

  SessionPtr RoomManager::require_session(
      const SessionId &sessionId) const
  {
    SessionPtr session =
        find_session(sessionId);

    if (!session)
    {
      throw Error{
          ErrorCode::SessionNotFound,
          "logical session was not found"};
    }

    return session;
  }

  bool RoomManager::has_session(
      const SessionId &sessionId) const
  {
    return find_session(sessionId) != nullptr;
  }

  std::vector<SessionId>
  RoomManager::session_ids() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    std::vector<SessionId> result;
    result.reserve(sessions_.size());

    for (const auto &[sessionId, session] : sessions_)
    {
      static_cast<void>(session);
      result.push_back(sessionId);
    }

    std::sort(
        result.begin(),
        result.end());

    return result;
  }

  std::size_t RoomManager::session_count() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return sessions_.size();
  }

  ConnectionPtr RoomManager::attach_connection(
      const SessionId &sessionId,
      ConnectionPtr connection,
      Timestamp now)
  {
    return attach_connection(
        require_session(sessionId),
        std::move(connection),
        now);
  }

  ConnectionPtr RoomManager::attach_connection(
      const SessionPtr &session,
      ConnectionPtr connection,
      Timestamp now)
  {
    if (!session)
    {
      throw Error{ErrorCode::SessionNotFound,
                  "connection attachment requires a session"};
    }

    ConnectionPtr previous =
        session->attach(
            std::move(connection),
            now);

    mark_session_present(session, now);
    return previous;
  }

  ConnectionPtr RoomManager::detach_connection(
      const SessionId &sessionId,
      const ConnectionId &connectionId,
      Timestamp now)
  {
    return detach_connection(
        require_session(sessionId),
        connectionId,
        now);
  }

  ConnectionPtr RoomManager::detach_connection(
      const SessionPtr &session,
      const ConnectionId &connectionId,
      Timestamp now)
  {
    if (!session)
    {
      throw Error{ErrorCode::SessionNotFound,
                  "connection detachment requires a session"};
    }

    ConnectionPtr detached =
        session->detach(
            connectionId,
            now);

    if (detached)
    {
      mark_session_detached(
          session,
          now);
    }

    return detached;
  }

  bool RoomManager::close_session(
      const SessionId &sessionId,
      ErrorCode code,
      std::string_view reason,
      Timestamp now)
  {
    SessionPtr session =
        find_session(sessionId);

    if (!session)
    {
      return false;
    }

    const auto joinedRooms =
        session->rooms();

    for (const auto &roomId : joinedRooms)
    {
      RoomPtr room =
          find_room(roomId);

      if (!room)
      {
        static_cast<void>(
            session->leave_room(roomId));

        continue;
      }

      if (!room->has_session(sessionId))
      {
        static_cast<void>(
            session->leave_room(roomId));

        continue;
      }

      CommandResult result =
          room->leave(sessionId);

      if (result.is_rejected())
      {
        throw Error{
            result.error_code().value_or(
                ErrorCode::CommandRejected),
            result.message().empty()
                ? "room rejected session cleanup"
                : result.message()};
      }

      static_cast<void>(
          session->leave_room(roomId));
    }

    if (presenceStore_)
    {
      static_cast<void>(
          presenceStore_->clear_session(
              sessionId));
    }

    ConnectionPtr connection =
        session->close(now);

    erase_session_if_same(
        sessionId,
        session);

    if (connection)
    {
      try
      {
        connection->close(
            code,
            reason);
      }
      catch (...)
      {
      }
    }

    return true;
  }

  CommandResult RoomManager::join_room(
      const SessionId &sessionId,
      const RoomId &roomId,
      Timestamp now)
  {
    SessionPtr session =
        require_session(sessionId);

    RoomPtr room =
        require_room(roomId);

    if (session->closed())
    {
      throw Error{
          ErrorCode::SessionExpired,
          "closed session cannot join a room"};
    }

    if (session->has_room(roomId))
    {
      throw Error{
          ErrorCode::AlreadyJoined,
          "session already belongs to the room"};
    }

    if (room->has_session(sessionId))
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room contains a membership missing from the session"};
    }

    if (session->room_count() >=
        config_.maxRoomsPerSession)
    {
      throw Error{
          ErrorCode::CommandRejected,
          "session reached its joined room limit"};
    }

    if (!session->join_room(roomId))
    {
      throw Error{
          ErrorCode::AlreadyJoined,
          "session already belongs to the room"};
    }

    bool presenceInserted = false;

    try
    {
      if (presenceStore_)
      {
        Presence presence{
            roomId,
            sessionId,
            session->identity(),
            nodeId_,
            session->connection_id(),
            now,
            session->metadata()};

        static_cast<void>(
            presenceStore_->upsert(
                std::move(presence)));

        presenceInserted = true;
      }

      CommandResult result =
          room->join(sessionId);

      if (result.is_rejected())
      {
        if (presenceInserted)
        {
          static_cast<void>(
              presenceStore_->erase(
                  roomId,
                  sessionId));
        }

        static_cast<void>(
            session->leave_room(roomId));

        return result;
      }

      return result;
    }
    catch (...)
    {
      if (presenceInserted)
      {
        try
        {
          static_cast<void>(
              presenceStore_->erase(
                  roomId,
                  sessionId));
        }
        catch (...)
        {
        }
      }

      static_cast<void>(
          session->leave_room(roomId));

      throw;
    }
  }

  CommandResult RoomManager::leave_room(
      const SessionId &sessionId,
      const RoomId &roomId,
      Timestamp now)
  {
    SessionPtr session =
        require_session(sessionId);

    RoomPtr room =
        require_room(roomId);

    if (!session->has_room(roomId) ||
        !room->has_session(sessionId))
    {
      throw Error{
          ErrorCode::MembershipNotFound,
          "session does not belong to the room"};
    }

    CommandResult result =
        room->leave(sessionId);

    if (result.is_rejected())
    {
      return result;
    }

    static_cast<void>(
        session->leave_room(roomId));

    if (presenceStore_)
    {
      const auto presence =
          presenceStore_->find(
              roomId,
              sessionId);

      if (presence)
      {
        static_cast<void>(
            presenceStore_->mark_left(
                roomId,
                sessionId,
                now));
      }
    }

    return result;
  }

  CommandResult RoomManager::execute(
      const RoomCommand &command)
  {
    command.validate();

    RoomPtr room =
        require_room(
            command.room_id());

    SessionPtr session =
        find_session(
            command.session_id());

    if (session &&
        (!session->has_room(
             command.room_id()) ||
         !room->has_session(
             command.session_id())))
    {
      throw Error{
          ErrorCode::MembershipNotFound,
          "command session does not belong to the room"};
    }

    if (session)
    {
      touch_presence(
          command.room_id(),
          command.session_id());
    }

    return room->execute(command);
  }

  CommandQueueStatus
  RoomManager::enqueue(RoomCommand command)
  {
    command.validate();

    RoomPtr room =
        require_room(
            command.room_id());

    SessionPtr session =
        find_session(
            command.session_id());

    if (session &&
        (!session->has_room(
             command.room_id()) ||
         !room->has_session(
             command.session_id())))
    {
      throw Error{
          ErrorCode::MembershipNotFound,
          "command session does not belong to the room"};
    }

    if (session)
    {
      touch_presence(
          command.room_id(),
          command.session_id());
    }

    return room->enqueue(
        std::move(command));
  }

  std::optional<CommandResult>
  RoomManager::process_next(
      const RoomId &roomId)
  {
    return require_room(roomId)
        ->process_next();
  }

  std::optional<Presence>
  RoomManager::find_presence(
      const RoomId &roomId,
      const SessionId &sessionId) const
  {
    if (!presenceStore_)
    {
      return std::nullopt;
    }

    return presenceStore_->find(
        roomId,
        sessionId);
  }

  std::vector<Presence>
  RoomManager::room_presence(
      const RoomId &roomId) const
  {
    if (!presenceStore_)
    {
      return {};
    }

    return presenceStore_->list_room(
        roomId);
  }

  std::size_t RoomManager::cleanup(
      Timestamp now)
  {
    if (config_.roomIdleTimeout.count() == 0)
    {
      return 0;
    }

    std::vector<RoomId> expired;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      for (const auto &[roomId, room] : rooms_)
      {
        if (!room ||
            !room->is_open() ||
            !room->empty())
        {
          continue;
        }

        const auto idleFor =
            now - room->last_activity_at();

        if (idleFor >= config_.roomIdleTimeout)
        {
          expired.push_back(roomId);
        }
      }
    }

    std::size_t removed = 0;

    for (const RoomId &roomId : expired)
    {
      CommandResult result =
          close_room(roomId, true);

      if (!result.is_rejected())
      {
        ++removed;
      }
    }

    return removed;
  }

  std::size_t RoomManager::cleanup_inactive(
      Timestamp now)
  {
    return cleanup(now);
  }

  void RoomManager::shutdown()
  {
    const std::vector<SessionId> sessions =
        session_ids();

    for (const SessionId &sessionId : sessions)
    {
      static_cast<void>(
          close_session(
              sessionId,
              ErrorCode::Cancelled,
              "room manager shutdown"));
    }

    const std::vector<RoomId> rooms =
        room_ids();

    for (const RoomId &roomId : rooms)
    {
      static_cast<void>(
          close_room(
              roomId,
              true));
    }
  }

  const NodeId &
  RoomManager::node_id() const noexcept
  {
    return nodeId_;
  }

  const Config &
  RoomManager::config() const noexcept
  {
    return config_;
  }

  const EventStorePtr &
  RoomManager::event_store() const noexcept
  {
    return eventStore_;
  }

  const SnapshotStorePtr &
  RoomManager::snapshot_store() const noexcept
  {
    return snapshotStore_;
  }

  const PresenceStorePtr &
  RoomManager::presence_store() const noexcept
  {
    return presenceStore_;
  }

  const std::shared_ptr<RoomDirectory> &
  RoomManager::room_directory() const noexcept
  {
    return roomDirectory_;
  }

  void RoomManager::deliver_event(
      const SessionId &sessionId,
      const RoomEvent &event) const
  {
    SessionPtr session =
        require_session(sessionId);

    session->send(
        protocol::from_event(event));
  }

  void RoomManager::mark_session_present(
      const SessionPtr &session,
      Timestamp now) noexcept
  {
    if (!presenceStore_ ||
        !session)
    {
      return;
    }

    const ConnectionId connectionId =
        session->connection_id();

    for (const auto &roomId : session->rooms())
    {
      try
      {
        const auto existing =
            presenceStore_->find(
                roomId,
                session->id());

        if (existing)
        {
          static_cast<void>(
              presenceStore_->mark_present(
                  roomId,
                  session->id(),
                  connectionId,
                  nodeId_,
                  now));
        }
        else
        {
          Presence presence{
              roomId,
              session->id(),
              session->identity(),
              nodeId_,
              connectionId,
              now,
              session->metadata()};

          static_cast<void>(
              presenceStore_->upsert(
                  std::move(presence)));
        }
      }
      catch (...)
      {
      }
    }
  }

  void RoomManager::mark_session_detached(
      const SessionPtr &session,
      Timestamp now) noexcept
  {
    if (!presenceStore_ ||
        !session)
    {
      return;
    }

    for (const auto &roomId : session->rooms())
    {
      try
      {
        const auto existing =
            presenceStore_->find(
                roomId,
                session->id());

        if (existing &&
            !existing->left())
        {
          static_cast<void>(
              presenceStore_->mark_detached(
                  roomId,
                  session->id(),
                  now));
        }
      }
      catch (...)
      {
      }
    }
  }

  void RoomManager::touch_presence(
      const RoomId &roomId,
      const SessionId &sessionId,
      Timestamp now) noexcept
  {
    if (!presenceStore_)
    {
      return;
    }

    try
    {
      const auto existing =
          presenceStore_->find(
              roomId,
              sessionId);

      if (existing &&
          !existing->left())
      {
        static_cast<void>(
            presenceStore_->touch(
                roomId,
                sessionId,
                now));
      }
    }
    catch (...)
    {
    }
  }

  void RoomManager::erase_room_if_same(
      const RoomId &roomId,
      const RoomPtr &room)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        rooms_.find(roomId);

    if (iterator != rooms_.end() &&
        iterator->second == room)
    {
      rooms_.erase(iterator);
    }
  }

  void RoomManager::erase_session_if_same(
      const SessionId &sessionId,
      const SessionPtr &session)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        sessions_.find(sessionId);

    if (iterator != sessions_.end() &&
        iterator->second == session)
    {
      sessions_.erase(iterator);
    }
  }

} // namespace vix::realtime
