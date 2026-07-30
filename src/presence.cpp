/**
 *
 * @file presence.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime logical room presence records.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/presence.hpp>

#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  namespace
  {
    /**
     * @brief Validate that a presence timestamp does not move backwards.
     */
    void validate_update_time(
        Timestamp joinedAt,
        Timestamp lastSeenAt,
        Timestamp now)
    {
      if (now < joinedAt ||
          now < lastSeenAt)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "presence timestamp moved backwards"};
      }
    }

  } // namespace

  Presence::Presence(
      RoomId roomId,
      SessionId sessionId,
      Identity identity,
      std::optional<NodeId> nodeId,
      ConnectionId connectionId,
      Timestamp joinedAt,
      JsonObject metadata)
      : roomId_(std::move(roomId)),
        sessionId_(std::move(sessionId)),
        identity_(std::move(identity)),
        nodeId_(std::move(nodeId)),
        connectionId_(std::move(connectionId)),
        status_(PresenceStatus::Present),
        joinedAt_(joinedAt),
        lastSeenAt_(joinedAt),
        detachedAt_(),
        leftAt_(),
        metadata_(std::move(metadata))
  {
    validate();
  }

  const RoomId &Presence::room_id() const noexcept
  {
    return roomId_;
  }

  const SessionId &
  Presence::session_id() const noexcept
  {
    return sessionId_;
  }

  const Identity &Presence::identity() const noexcept
  {
    return identity_;
  }

  const std::optional<NodeId> &
  Presence::node_id() const noexcept
  {
    return nodeId_;
  }

  const ConnectionId &
  Presence::connection_id() const noexcept
  {
    return connectionId_;
  }

  PresenceStatus Presence::status() const noexcept
  {
    return status_;
  }

  Timestamp Presence::joined_at() const noexcept
  {
    return joinedAt_;
  }

  Timestamp Presence::last_seen_at() const noexcept
  {
    return lastSeenAt_;
  }

  const std::optional<Timestamp> &
  Presence::detached_at() const noexcept
  {
    return detachedAt_;
  }

  const std::optional<Timestamp> &
  Presence::left_at() const noexcept
  {
    return leftAt_;
  }

  bool Presence::logically_present() const noexcept
  {
    return status_ == PresenceStatus::Present ||
           status_ == PresenceStatus::Detached;
  }

  bool Presence::connected() const noexcept
  {
    return status_ == PresenceStatus::Present &&
           !connectionId_.empty();
  }

  bool Presence::detached() const noexcept
  {
    return status_ == PresenceStatus::Detached;
  }

  bool Presence::left() const noexcept
  {
    return status_ == PresenceStatus::Left;
  }

  bool Presence::stale(
      Timestamp now,
      std::chrono::milliseconds timeout) const noexcept
  {
    if (status_ == PresenceStatus::Left ||
        timeout.count() < 0)
    {
      return true;
    }

    if (now < lastSeenAt_)
    {
      return false;
    }

    const auto inactiveDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastSeenAt_);

    return inactiveDuration >= timeout;
  }

  bool Presence::active(
      Timestamp now,
      std::chrono::milliseconds timeout) const noexcept
  {
    return logically_present() &&
           !stale(now, timeout);
  }

  Presence &Presence::touch(Timestamp now)
  {
    if (status_ == PresenceStatus::Left)
    {
      throw Error{
          ErrorCode::MembershipNotFound,
          "left presence cannot receive activity updates"};
    }

    validate_update_time(
        joinedAt_,
        lastSeenAt_,
        now);

    lastSeenAt_ = now;
    return *this;
  }

  Presence &Presence::mark_present(
      ConnectionId connectionId,
      std::optional<NodeId> nodeId,
      Timestamp now)
  {
    if (status_ == PresenceStatus::Left)
    {
      throw Error{
          ErrorCode::MembershipNotFound,
          "left presence cannot become present again"};
    }

    validate_update_time(
        joinedAt_,
        lastSeenAt_,
        now);

    if (nodeId.has_value())
    {
      if (nodeId->empty())
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "presence node identifier cannot be empty"};
      }

      nodeId_ = std::move(nodeId);
    }

    connectionId_ = std::move(connectionId);
    status_ = PresenceStatus::Present;
    detachedAt_.reset();
    lastSeenAt_ = now;

    return *this;
  }

  Presence &Presence::mark_detached(Timestamp now)
  {
    if (status_ == PresenceStatus::Left)
    {
      throw Error{
          ErrorCode::MembershipNotFound,
          "left presence cannot become detached"};
    }

    validate_update_time(
        joinedAt_,
        lastSeenAt_,
        now);

    connectionId_.clear();
    status_ = PresenceStatus::Detached;
    detachedAt_ = now;
    lastSeenAt_ = now;

    return *this;
  }

  Presence &Presence::mark_left(Timestamp now)
  {
    if (status_ == PresenceStatus::Left)
    {
      return *this;
    }

    validate_update_time(
        joinedAt_,
        lastSeenAt_,
        now);

    connectionId_.clear();
    status_ = PresenceStatus::Left;
    lastSeenAt_ = now;
    leftAt_ = now;

    return *this;
  }

  Presence &Presence::set_node_id(NodeId nodeId)
  {
    if (nodeId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "presence node identifier cannot be empty"};
    }

    if (status_ == PresenceStatus::Left)
    {
      throw Error{
          ErrorCode::MembershipNotFound,
          "left presence cannot change its reporting node"};
    }

    nodeId_ = std::move(nodeId);
    return *this;
  }

  Presence &Presence::clear_node_id() noexcept
  {
    nodeId_.reset();
    return *this;
  }

  Presence &Presence::set_connection_id(
      ConnectionId connectionId,
      Timestamp now)
  {
    if (connectionId.empty())
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "presence connection identifier cannot be empty"};
    }

    return mark_present(
        std::move(connectionId),
        std::nullopt,
        now);
  }

  Presence &Presence::clear_connection_id(
      Timestamp now)
  {
    return mark_detached(now);
  }

  const JsonObject &
  Presence::metadata() const noexcept
  {
    return metadata_;
  }

  Presence &Presence::set_metadata(JsonObject value)
  {
    metadata_ = std::move(value);
    return *this;
  }

  bool Presence::is_valid() const noexcept
  {
    if (roomId_.empty() ||
        sessionId_.empty())
    {
      return false;
    }

    if (nodeId_.has_value() &&
        nodeId_->empty())
    {
      return false;
    }

    if (lastSeenAt_ < joinedAt_)
    {
      return false;
    }

    switch (status_)
    {
    case PresenceStatus::Present:
      return !detachedAt_.has_value() &&
             !leftAt_.has_value();

    case PresenceStatus::Detached:
      return connectionId_.empty() &&
             detachedAt_.has_value() &&
             *detachedAt_ >= joinedAt_ &&
             *detachedAt_ <= lastSeenAt_ &&
             !leftAt_.has_value();

    case PresenceStatus::Left:
      return connectionId_.empty() &&
             leftAt_.has_value() &&
             *leftAt_ >= joinedAt_ &&
             *leftAt_ == lastSeenAt_;
    }

    return false;
  }

  void Presence::validate() const
  {
    if (roomId_.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "presence requires a room identifier"};
    }

    if (sessionId_.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "presence requires a session identifier"};
    }

    if (nodeId_.has_value() &&
        nodeId_->empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "presence node identifier cannot be empty"};
    }

    if (lastSeenAt_ < joinedAt_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "presence activity timestamp precedes room join"};
    }

    switch (status_)
    {
    case PresenceStatus::Present:
    {
      if (detachedAt_.has_value())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "present presence cannot retain a detachment timestamp"};
      }

      if (leftAt_.has_value())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "present presence cannot retain a leave timestamp"};
      }

      return;
    }

    case PresenceStatus::Detached:
    {
      if (!connectionId_.empty())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "detached presence cannot retain a connection identifier"};
      }

      if (!detachedAt_.has_value())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "detached presence requires a detachment timestamp"};
      }

      if (*detachedAt_ < joinedAt_ ||
          *detachedAt_ > lastSeenAt_)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "presence detachment timestamp is inconsistent"};
      }

      if (leftAt_.has_value())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "detached presence cannot retain a leave timestamp"};
      }

      return;
    }

    case PresenceStatus::Left:
    {
      if (!connectionId_.empty())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "left presence cannot retain a connection identifier"};
      }

      if (!leftAt_.has_value())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "left presence requires a leave timestamp"};
      }

      if (*leftAt_ < joinedAt_ ||
          *leftAt_ != lastSeenAt_)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "presence leave timestamp is inconsistent"};
      }

      return;
    }
    }

    throw Error{
        ErrorCode::CorruptedState,
        "presence contains an unknown lifecycle status"};
  }

} // namespace vix::realtime
