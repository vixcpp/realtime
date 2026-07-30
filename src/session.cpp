/**
 *
 * @file server.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the transport-independent Vix Realtime server.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/server.hpp>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  Server::Server(
      NodeId nodeId,
      Config config)
      : manager_(
            std::make_shared<RoomManager>(
                std::move(nodeId),
                std::move(config))),
        status_(ServerStatus::Created)
  {
  }

  Server::Server(RoomManagerPtr manager)
      : manager_(std::move(manager)),
        status_(ServerStatus::Created)
  {
    if (!manager_)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "realtime server requires a room manager"};
    }
  }

  Server::~Server()
  {
    try
    {
      static_cast<void>(stop());
    }
    catch (...)
    {
    }
  }

  bool Server::start()
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (status_ == ServerStatus::Running)
    {
      return false;
    }

    if (status_ == ServerStatus::Stopping)
    {
      throw Error{
          ErrorCode::RoomNotReady,
          "realtime server is currently stopping"};
    }

    if (status_ == ServerStatus::Failed)
    {
      throw Error{
          ErrorCode::InternalError,
          "failed realtime server cannot be restarted"};
    }

    status_ = ServerStatus::Running;
    return true;
  }

  bool Server::stop()
  {
    {
      std::lock_guard<std::mutex> lock{mutex_};

      if (status_ == ServerStatus::Stopped)
      {
        return false;
      }

      if (status_ == ServerStatus::Created)
      {
        status_ = ServerStatus::Stopped;
        return false;
      }

      if (status_ == ServerStatus::Stopping)
      {
        return false;
      }

      status_ = ServerStatus::Stopping;
    }

    std::size_t failures = 0;

    const std::vector<SessionId> sessionIds =
        manager_->session_ids();

    for (const auto &sessionId : sessionIds)
    {
      try
      {
        static_cast<void>(
            manager_->close_session(
                sessionId,
                ErrorCode::Cancelled,
                "realtime server shutdown",
                SystemClock::now()));
      }
      catch (...)
      {
        ++failures;
      }
    }

    const std::vector<RoomId> roomIds =
        manager_->room_ids();

    for (const auto &roomId : roomIds)
    {
      try
      {
        CommandResult result =
            manager_->close_room(
                roomId,
                true);

        if (result.is_rejected())
        {
          ++failures;
        }
      }
      catch (...)
      {
        ++failures;
      }
    }

    {
      std::lock_guard<std::mutex> lock{mutex_};

      status_ =
          failures == 0
              ? ServerStatus::Stopped
              : ServerStatus::Failed;
    }

    if (failures != 0)
    {
      throw Error{
          ErrorCode::InternalError,
          "realtime server shutdown completed with cleanup failures"};
    }

    return true;
  }

  ServerStatus Server::status() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return status_;
  }

  bool Server::running() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return status_ == ServerStatus::Running;
  }

  bool Server::stopped() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return status_ == ServerStatus::Stopped;
  }

  bool Server::register_factory(
      RoomFactoryPtr factory,
      bool replace)
  {
    require_registry_available();

    return manager_->register_factory(
        std::move(factory),
        replace);
  }

  bool Server::unregister_factory(
      std::string_view roomType)
  {
    require_registry_available();

    return manager_->unregister_factory(
        roomType);
  }

  RoomPtr Server::open_room(
      RoomId roomId,
      std::string_view roomType,
      JsonObject metadata)
  {
    require_running();

    return manager_->open_room(
        std::move(roomId),
        roomType,
        std::move(metadata));
  }

  CommandResult Server::close_room(
      const RoomId &roomId,
      bool remove)
  {
    require_running();

    return manager_->close_room(
        roomId,
        remove);
  }

  RoomPtr Server::find_room(
      const RoomId &roomId) const
  {
    return manager_->find_room(roomId);
  }

  SessionPtr Server::create_session(
      SessionId sessionId,
      Identity identity,
      ResumeToken resumeToken,
      JsonObject metadata,
      Timestamp now)
  {
    require_running();

    return manager_->create_session(
        std::move(sessionId),
        std::move(identity),
        std::move(resumeToken),
        std::move(metadata),
        now);
  }

  SessionPtr Server::connect(
      SessionId sessionId,
      ConnectionPtr connection,
      Identity identity,
      ResumeToken resumeToken,
      JsonObject metadata,
      Timestamp now)
  {
    require_running();

    if (sessionId.empty())
    {
      throw Error{
          ErrorCode::SessionNotFound,
          "connection attachment requires a session identifier"};
    }

    if (!connection)
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "connection attachment requires a connection"};
    }

    SessionPtr session =
        manager_->find_session(sessionId);

    bool created = false;

    if (!session)
    {
      try
      {
        session =
            manager_->create_session(
                sessionId,
                std::move(identity),
                std::move(resumeToken),
                std::move(metadata),
                now);

        created = true;
      }
      catch (...)
      {
        session =
            manager_->find_session(sessionId);

        if (!session)
        {
          throw;
        }
      }
    }
    else
    {
      if (!identity.empty() &&
          session->identity() != identity)
      {
        throw Error{
            ErrorCode::Unauthorized,
            "connection identity does not match the logical session"};
      }

      if (!resumeToken.empty())
      {
        session->set_resume_token(
            std::move(resumeToken));
      }
    }

    ConnectionPtr previous;

    try
    {
      previous =
          manager_->attach_connection(
              sessionId,
              std::move(connection),
              now);
    }
    catch (...)
    {
      if (created)
      {
        try
        {
          static_cast<void>(
              manager_->close_session(
                  sessionId,
                  ErrorCode::ConnectionNotAttached,
                  "connection attachment failed",
                  now));
        }
        catch (...)
        {
        }
      }

      throw;
    }

    if (previous)
    {
      try
      {
        previous->close(
            ErrorCode::Cancelled,
            "connection replaced by a newer attachment");
      }
      catch (...)
      {
      }
    }

    return session;
  }

  ConnectionPtr Server::disconnect(
      const SessionId &sessionId,
      const ConnectionId &connectionId,
      Timestamp now)
  {
    require_running();

    return manager_->detach_connection(
        sessionId,
        connectionId,
        now);
  }

  bool Server::close_session(
      const SessionId &sessionId,
      ErrorCode code,
      std::string_view reason,
      Timestamp now)
  {
    require_running();

    return manager_->close_session(
        sessionId,
        code,
        reason,
        now);
  }

  SessionPtr Server::find_session(
      const SessionId &sessionId) const
  {
    return manager_->find_session(
        sessionId);
  }

  CommandResult Server::join_room(
      const SessionId &sessionId,
      const RoomId &roomId,
      Timestamp now)
  {
    require_running();

    return manager_->join_room(
        sessionId,
        roomId,
        now);
  }

  CommandResult Server::leave_room(
      const SessionId &sessionId,
      const RoomId &roomId,
      Timestamp now)
  {
    require_running();

    return manager_->leave_room(
        sessionId,
        roomId,
        now);
  }

  CommandResult Server::execute(
      const RoomCommand &command)
  {
    require_running();

    SessionPtr session =
        manager_->require_session(
            command.session_id());

    session->touch(
        SystemClock::now());

    return manager_->execute(command);
  }

  internal::CommandQueueStatus
  Server::enqueue(RoomCommand command)
  {
    require_running();

    SessionPtr session =
        manager_->require_session(
            command.session_id());

    session->touch(
        SystemClock::now());

    return manager_->enqueue(
        std::move(command));
  }

  std::optional<CommandResult>
  Server::process_next(
      const RoomId &roomId)
  {
    require_running();

    return manager_->process_next(
        roomId);
  }

  void Server::send(
      const SessionId &sessionId,
      const protocol::Envelope &envelope) const
  {
    require_running();

    manager_->require_session(
                sessionId)
        ->send(envelope);
  }

  std::size_t Server::prune_expired_sessions(
      Timestamp now)
  {
    require_running();

    const Config &runtimeConfig =
        manager_->config();

    const auto resumeWindow =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            runtimeConfig.sessionResumeWindow);

    const std::vector<SessionId> sessionIds =
        manager_->session_ids();

    std::size_t removed = 0;

    for (const auto &sessionId : sessionIds)
    {
      SessionPtr session =
          manager_->find_session(sessionId);

      if (!session)
      {
        continue;
      }

      if (session->connected())
      {
        continue;
      }

      if (runtimeConfig.enableSessionResume &&
          session->can_resume(
              now,
              resumeWindow))
      {
        continue;
      }

      try
      {
        if (manager_->close_session(
                sessionId,
                ErrorCode::SessionExpired,
                "session resume window expired",
                now))
        {
          ++removed;
        }
      }
      catch (...)
      {
      }
    }

    return removed;
  }

  std::size_t Server::prune_stale_presence(
      Timestamp now)
  {
    require_running();

    const PresenceStorePtr &store =
        manager_->presence_store();

    if (!store)
    {
      return 0;
    }

    const auto timeout =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            manager_->config().presenceTimeout);

    return store->prune_stale(
        now,
        timeout);
  }

  const RoomManagerPtr &
  Server::manager() const noexcept
  {
    return manager_;
  }

  const NodeId &
  Server::node_id() const noexcept
  {
    return manager_->node_id();
  }

  const Config &
  Server::config() const noexcept
  {
    return manager_->config();
  }

  void Server::require_running() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (status_ != ServerStatus::Running)
    {
      throw Error{
          ErrorCode::RoomNotReady,
          "realtime server is not running"};
    }
  }

  void Server::require_registry_available() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (status_ == ServerStatus::Stopping)
    {
      throw Error{
          ErrorCode::RoomNotReady,
          "realtime server is stopping"};
    }

    if (status_ == ServerStatus::Failed)
    {
      throw Error{
          ErrorCode::InternalError,
          "realtime server is in a failed state"};
    }
  }

} // namespace vix::realtime
