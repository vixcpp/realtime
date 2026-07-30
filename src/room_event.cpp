/**
 *
 * @file room_event.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime room events.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/room_event.hpp>

#include <cctype>
#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  RoomEvent::RoomEvent(
      RoomId roomId,
      std::string type,
      JsonObject payload,
      EventAudience audience)
      : eventId_(),
        roomId_(std::move(roomId)),
        roomVersion_(),
        type_(std::move(type)),
        payload_(std::move(payload)),
        audience_(audience),
        targetSession_(),
        sourceSession_(),
        requestId_(),
        correlationId_(),
        schemaVersion_(1),
        createdAt_(SystemClock::now()),
        metadata_()
  {
    validate();
  }

  EventId RoomEvent::event_id() const noexcept
  {
    return eventId_;
  }

  const RoomId &RoomEvent::room_id() const noexcept
  {
    return roomId_;
  }

  RoomVersion RoomEvent::room_version() const noexcept
  {
    return roomVersion_;
  }

  const std::string &RoomEvent::type() const noexcept
  {
    return type_;
  }

  const JsonObject &RoomEvent::payload() const noexcept
  {
    return payload_;
  }

  JsonObject &RoomEvent::payload() noexcept
  {
    return payload_;
  }

  EventAudience RoomEvent::audience() const noexcept
  {
    return audience_;
  }

  const std::optional<SessionId> &
  RoomEvent::target_session() const noexcept
  {
    return targetSession_;
  }

  const std::optional<SessionId> &
  RoomEvent::source_session() const noexcept
  {
    return sourceSession_;
  }

  const RequestId &RoomEvent::request_id() const noexcept
  {
    return requestId_;
  }

  const CorrelationId &RoomEvent::correlation_id() const noexcept
  {
    return correlationId_;
  }

  SchemaVersion RoomEvent::schema_version() const noexcept
  {
    return schemaVersion_;
  }

  Timestamp RoomEvent::created_at() const noexcept
  {
    return createdAt_;
  }

  const JsonObject &RoomEvent::metadata() const noexcept
  {
    return metadata_;
  }

  RoomEvent &RoomEvent::set_event_id(EventId value) noexcept
  {
    eventId_ = value;
    return *this;
  }

  RoomEvent &RoomEvent::set_room_version(
      RoomVersion value) noexcept
  {
    roomVersion_ = value;
    return *this;
  }

  RoomEvent &RoomEvent::set_audience(
      EventAudience value) noexcept
  {
    audience_ = value;
    return *this;
  }

  RoomEvent &RoomEvent::set_target_session(SessionId value)
  {
    targetSession_ = std::move(value);
    return *this;
  }

  RoomEvent &RoomEvent::clear_target_session() noexcept
  {
    targetSession_.reset();
    return *this;
  }

  RoomEvent &RoomEvent::set_source_session(SessionId value)
  {
    sourceSession_ = std::move(value);
    return *this;
  }

  RoomEvent &RoomEvent::clear_source_session() noexcept
  {
    sourceSession_.reset();
    return *this;
  }

  RoomEvent &RoomEvent::set_request_id(RequestId value)
  {
    requestId_ = std::move(value);
    return *this;
  }

  RoomEvent &RoomEvent::set_correlation_id(
      CorrelationId value)
  {
    correlationId_ = std::move(value);
    return *this;
  }

  RoomEvent &RoomEvent::set_schema_version(
      SchemaVersion value)
  {
    if (value == 0)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room event schema version cannot be zero"};
    }

    schemaVersion_ = value;
    return *this;
  }

  RoomEvent &RoomEvent::set_created_at(
      Timestamp value) noexcept
  {
    createdAt_ = value;
    return *this;
  }

  RoomEvent &RoomEvent::set_metadata(JsonObject value)
  {
    metadata_ = std::move(value);
    return *this;
  }

  bool RoomEvent::is_valid() const noexcept
  {
    if (roomId_.empty() ||
        !is_valid_type(type_) ||
        schemaVersion_ == 0)
    {
      return false;
    }

    if (audience_ == EventAudience::Session)
    {
      return targetSession_.has_value() &&
             !targetSession_->empty();
    }

    return !targetSession_.has_value();
  }

  void RoomEvent::validate() const
  {
    if (roomId_.empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room event requires a room identifier"};
    }

    if (type_.empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room event type cannot be empty"};
    }

    if (type_.size() > max_type_size)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room event type exceeds the maximum size of 128 characters"};
    }

    if (!is_valid_type(type_))
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room event type is invalid"};
    }

    if (schemaVersion_ == 0)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room event schema version cannot be zero"};
    }

    if (audience_ == EventAudience::Session)
    {
      if (!targetSession_.has_value() ||
          targetSession_->empty())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "session-scoped room event requires a target session"};
      }
    }
    else if (targetSession_.has_value())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room event target session requires session audience"};
    }
  }

  bool RoomEvent::is_valid_type(
      std::string_view value) noexcept
  {
    if (value.empty() || value.size() > max_type_size)
    {
      return false;
    }

    if (value.front() == '.' || value.back() == '.')
    {
      return false;
    }

    bool previousWasDot = false;

    for (const char character : value)
    {
      const auto byte = static_cast<unsigned char>(character);

      const bool isAlphaNumeric =
          std::isalnum(byte) != 0;

      const bool isAllowedSeparator =
          character == '.' ||
          character == '-' ||
          character == '_';

      if (!isAlphaNumeric && !isAllowedSeparator)
      {
        return false;
      }

      const bool currentIsDot = character == '.';

      if (currentIsDot && previousWasDot)
      {
        return false;
      }

      previousWasDot = currentIsDot;
    }

    return true;
  }

} // namespace vix::realtime
