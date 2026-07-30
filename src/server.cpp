/**
 *
 * @file session.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime logical client sessions.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/session.hpp>

#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  Session::Session(
      SessionId sessionId,
      Identity identity,
      ResumeToken resumeToken,
      Timestamp createdAt,
      JsonObject metadata)
      : sessionId_(std::move(sessionId)),
        identity_(std::move(identity)),
        createdAt_(createdAt),
        connection_(),
        resumeToken_(std::move(resumeToken)),
        lastSeenAt_(createdAt),
        detachedAt_(),
        closedAt_(),
        rooms_(),
        metadata_(std::move(metadata))
  {
    validate();
  }

  const SessionId &Session::id() const noexcept
  {
    return sessionId_;
  }

  const Identity &Session::identity() const noexcept
  {
    return identity_;
  }

  Timestamp Session::created_at() const noexcept
  {
    return createdAt_;
  }

  SessionStatus Session::status() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (closedAt_)
    {
      return SessionStatus::Closed;
    }

    if (connection_ && connection_->is_open())
    {
      return SessionStatus::Connected;
    }

    return SessionStatus::Detached;
  }

  bool Session::connected() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return !closedAt_.has_value() &&
           connection_ != nullptr &&
           connection_->is_open();
  }

  bool Session::closed() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return closedAt_.has_value();
  }

  ConnectionPtr Session::connection() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return connection_;
  }

  ConnectionId Session::connection_id() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (!connection_)
    {
      return {};
    }

    return connection_->id();
  }

  ConnectionPtr Session::attach(
      ConnectionPtr connection,
      Timestamp now)
  {
    if (!connection)
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "session cannot attach a null connection"};
    }

    if (connection->id().empty())
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "session cannot attach a connection without an identifier"};
    }

    if (!connection->is_open())
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "session cannot attach a closed connection"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    if (closedAt_)
    {
      throw Error{
          ErrorCode::SessionExpired,
          "closed session cannot attach a connection"};
    }

    ConnectionPtr previous =
        std::move(connection_);

    connection_ = std::move(connection);
    detachedAt_.reset();
    lastSeenAt_ = now;

    return previous;
  }

  ConnectionPtr Session::detach(
      const ConnectionId &connectionId,
      Timestamp now)
  {
    if (connectionId.empty())
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "connection detachment requires an identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    if (!connection_ ||
        connection_->id() != connectionId)
    {
      return {};
    }

    ConnectionPtr detached =
        std::move(connection_);

    connection_.reset();
    detachedAt_ = now;
    lastSeenAt_ = now;

    return detached;
  }

  ConnectionPtr Session::detach(Timestamp now)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (!connection_)
    {
      return {};
    }

    ConnectionPtr detached =
        std::move(connection_);

    connection_.reset();
    detachedAt_ = now;
    lastSeenAt_ = now;

    return detached;
  }

  ConnectionPtr Session::close(Timestamp now)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (closedAt_)
    {
      return {};
    }

    ConnectionPtr previous =
        std::move(connection_);

    connection_.reset();
    detachedAt_ = now;
    closedAt_ = now;
    lastSeenAt_ = now;
    resumeToken_.clear();

    return previous;
  }

  void Session::send(
      const protocol::Envelope &envelope) const
  {
    ConnectionPtr activeConnection;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      if (closedAt_)
      {
        throw Error{
            ErrorCode::SessionExpired,
            "closed session cannot send protocol messages"};
      }

      if (!connection_)
      {
        throw Error{
            ErrorCode::ConnectionNotAttached,
            "session has no active connection"};
      }

      activeConnection = connection_;
    }

    if (!activeConnection->is_open())
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "session connection is no longer open"};
    }

    activeConnection->send(envelope);
  }

  void Session::touch(Timestamp now)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (!closedAt_)
    {
      lastSeenAt_ = now;
    }
  }

  Timestamp Session::last_seen_at() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return lastSeenAt_;
  }

  std::optional<Timestamp>
  Session::detached_at() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return detachedAt_;
  }

  std::optional<Timestamp>
  Session::closed_at() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return closedAt_;
  }

  ResumeToken Session::resume_token() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return resumeToken_;
  }

  void Session::set_resume_token(
      ResumeToken value)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (closedAt_)
    {
      throw Error{
          ErrorCode::SessionExpired,
          "closed session cannot receive a resume token"};
    }

    resumeToken_ = std::move(value);
  }

  void Session::clear_resume_token()
  {
    std::lock_guard<std::mutex> lock{mutex_};

    resumeToken_.clear();
  }

  bool Session::can_resume(
      Timestamp now,
      std::chrono::milliseconds resumeWindow) const
  {
    if (resumeWindow.count() < 0)
    {
      return false;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    if (closedAt_ ||
        resumeToken_.empty() ||
        !detachedAt_)
    {
      return false;
    }

    if (connection_ && connection_->is_open())
    {
      return false;
    }

    if (now < *detachedAt_)
    {
      return false;
    }

    const auto detachedDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - *detachedAt_);

    return detachedDuration <= resumeWindow;
  }

  bool Session::join_room(const RoomId &roomId)
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::MembershipNotFound,
          "session cannot join an empty room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    if (closedAt_)
    {
      throw Error{
          ErrorCode::SessionExpired,
          "closed session cannot join a room"};
    }

    return rooms_.insert(roomId).second;
  }

  bool Session::leave_room(const RoomId &roomId)
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::MembershipNotFound,
          "session cannot leave an empty room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    return rooms_.erase(roomId) != 0;
  }

  bool Session::has_room(
      const RoomId &roomId) const
  {
    if (roomId.empty())
    {
      return false;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    return rooms_.contains(roomId);
  }

  std::vector<RoomId> Session::rooms() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return {
        rooms_.begin(),
        rooms_.end()};
  }

  std::size_t Session::room_count() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return rooms_.size();
  }

  std::size_t Session::clear_rooms()
  {
    std::lock_guard<std::mutex> lock{mutex_};

    const std::size_t removed =
        rooms_.size();

    rooms_.clear();
    return removed;
  }

  JsonObject Session::metadata() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return metadata_;
  }

  void Session::set_metadata(
      JsonObject value)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    metadata_ = std::move(value);
  }

  bool Session::is_valid() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (sessionId_.empty())
    {
      return false;
    }

    if (connection_ &&
        connection_->id().empty())
    {
      return false;
    }

    if (closedAt_ && connection_)
    {
      return false;
    }

    if (closedAt_ &&
        !resumeToken_.empty())
    {
      return false;
    }

    if (connection_ && detachedAt_)
    {
      return false;
    }

    if (lastSeenAt_ < createdAt_)
    {
      return false;
    }

    if (detachedAt_ &&
        *detachedAt_ < createdAt_)
    {
      return false;
    }

    if (closedAt_ &&
        *closedAt_ < createdAt_)
    {
      return false;
    }

    return true;
  }

  void Session::validate() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    if (sessionId_.empty())
    {
      throw Error{
          ErrorCode::SessionNotFound,
          "logical session requires an identifier"};
    }

    if (connection_ &&
        connection_->id().empty())
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "attached connection requires an identifier"};
    }

    if (closedAt_ && connection_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "closed session cannot retain an active connection"};
    }

    if (closedAt_ &&
        !resumeToken_.empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "closed session cannot retain a resume token"};
    }

    if (connection_ && detachedAt_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "connected session cannot retain a detachment timestamp"};
    }

    if (lastSeenAt_ < createdAt_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "session activity timestamp precedes session creation"};
    }

    if (detachedAt_ &&
        *detachedAt_ < createdAt_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "session detachment timestamp precedes session creation"};
    }

    if (closedAt_ &&
        *closedAt_ < createdAt_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "session closure timestamp precedes session creation"};
    }
  }

} // namespace vix::realtime
