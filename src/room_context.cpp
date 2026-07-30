/**
 *
 * @file room_context.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime room execution contexts.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/room_context.hpp>

#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  RoomContext::RoomContext(
      RoomId roomId,
      RoomVersion roomVersion,
      EventId lastEventId,
      std::optional<SessionId> sessionId,
      RequestId requestId,
      CorrelationId correlationId,
      std::optional<NodeId> nodeId,
      Timestamp now,
      JsonObject metadata)
      : roomId_(std::move(roomId)),
        roomVersion_(roomVersion),
        lastEventId_(lastEventId),
        sessionId_(std::move(sessionId)),
        requestId_(std::move(requestId)),
        correlationId_(std::move(correlationId)),
        nodeId_(std::move(nodeId)),
        now_(now),
        metadata_(std::move(metadata))
  {
    validate();
  }

  RoomContext RoomContext::from_command(
      const RoomCommand &command,
      RoomVersion roomVersion,
      EventId lastEventId,
      std::optional<NodeId> nodeId,
      Timestamp now,
      JsonObject metadata)
  {
    command.validate();

    return RoomContext{
        command.room_id(),
        roomVersion,
        lastEventId,
        command.session_id(),
        command.request_id(),
        command.correlation_id(),
        std::move(nodeId),
        now,
        std::move(metadata)};
  }

  const RoomId &RoomContext::room_id() const noexcept
  {
    return roomId_;
  }

  RoomVersion RoomContext::room_version() const noexcept
  {
    return roomVersion_;
  }

  EventId RoomContext::last_event_id() const noexcept
  {
    return lastEventId_;
  }

  RoomVersion RoomContext::next_room_version() const
  {
    return roomVersion_.next();
  }

  EventId RoomContext::next_event_id() const
  {
    return lastEventId_.next();
  }

  const std::optional<SessionId> &
  RoomContext::session_id() const noexcept
  {
    return sessionId_;
  }

  const RequestId &
  RoomContext::request_id() const noexcept
  {
    return requestId_;
  }

  const CorrelationId &
  RoomContext::correlation_id() const noexcept
  {
    return correlationId_;
  }

  const std::optional<NodeId> &
  RoomContext::node_id() const noexcept
  {
    return nodeId_;
  }

  Timestamp RoomContext::now() const noexcept
  {
    return now_;
  }

  const JsonObject &
  RoomContext::metadata() const noexcept
  {
    return metadata_;
  }

  bool RoomContext::is_valid() const noexcept
  {
    if (roomId_.empty())
    {
      return false;
    }

    if (sessionId_.has_value() &&
        sessionId_->empty())
    {
      return false;
    }

    if (nodeId_.has_value() &&
        nodeId_->empty())
    {
      return false;
    }

    if (roomVersion_.is_initial() &&
        !lastEventId_.empty())
    {
      return false;
    }

    if (!roomVersion_.is_initial() &&
        lastEventId_.empty())
    {
      return false;
    }

    return true;
  }

  void RoomContext::validate() const
  {
    if (roomId_.empty())
    {
      throw Error{
          ErrorCode::InternalError,
          "room context requires a room identifier"};
    }

    if (sessionId_.has_value() &&
        sessionId_->empty())
    {
      throw Error{
          ErrorCode::InternalError,
          "room context cannot contain an empty session identifier"};
    }

    if (nodeId_.has_value() &&
        nodeId_->empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room context cannot contain an empty node identifier"};
    }

    if (roomVersion_.is_initial() &&
        !lastEventId_.empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "initial room context cannot reference a persisted event"};
    }

    if (!roomVersion_.is_initial() &&
        lastEventId_.empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "versioned room context requires a last event identifier"};
    }
  }

} // namespace vix::realtime
