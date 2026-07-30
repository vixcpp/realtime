/**
 *
 * @file room_command.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime room commands.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/room_command.hpp>

#include <cctype>
#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  RoomCommand::RoomCommand(
      RoomId roomId,
      SessionId sessionId,
      std::string type,
      JsonObject payload,
      RequestId requestId)
      : roomId_(std::move(roomId)),
        sessionId_(std::move(sessionId)),
        type_(std::move(type)),
        payload_(std::move(payload)),
        requestId_(std::move(requestId)),
        correlationId_(),
        expectedVersion_(),
        createdAt_(SystemClock::now()),
        metadata_()
  {
    validate();
  }

  const RoomId &RoomCommand::room_id() const noexcept
  {
    return roomId_;
  }

  const SessionId &RoomCommand::session_id() const noexcept
  {
    return sessionId_;
  }

  const std::string &RoomCommand::type() const noexcept
  {
    return type_;
  }

  const JsonObject &RoomCommand::payload() const noexcept
  {
    return payload_;
  }

  JsonObject &RoomCommand::payload() noexcept
  {
    return payload_;
  }

  const RequestId &RoomCommand::request_id() const noexcept
  {
    return requestId_;
  }

  const CorrelationId &RoomCommand::correlation_id() const noexcept
  {
    return correlationId_;
  }

  const std::optional<RoomVersion> &
  RoomCommand::expected_version() const noexcept
  {
    return expectedVersion_;
  }

  Timestamp RoomCommand::created_at() const noexcept
  {
    return createdAt_;
  }

  const JsonObject &RoomCommand::metadata() const noexcept
  {
    return metadata_;
  }

  RoomCommand &RoomCommand::set_correlation_id(
      CorrelationId value)
  {
    correlationId_ = std::move(value);
    return *this;
  }

  RoomCommand &RoomCommand::set_expected_version(
      RoomVersion version)
  {
    expectedVersion_ = version;
    return *this;
  }

  RoomCommand &RoomCommand::clear_expected_version() noexcept
  {
    expectedVersion_.reset();
    return *this;
  }

  RoomCommand &RoomCommand::set_created_at(
      Timestamp value) noexcept
  {
    createdAt_ = value;
    return *this;
  }

  RoomCommand &RoomCommand::set_metadata(JsonObject value)
  {
    metadata_ = std::move(value);
    return *this;
  }

  bool RoomCommand::is_valid() const noexcept
  {
    return !roomId_.empty() &&
           !sessionId_.empty() &&
           is_valid_type(type_);
  }

  void RoomCommand::validate() const
  {
    if (roomId_.empty())
    {
      throw Error{
          ErrorCode::InvalidCommand,
          "room command requires a room identifier"};
    }

    if (sessionId_.empty())
    {
      throw Error{
          ErrorCode::InvalidCommand,
          "room command requires a session identifier"};
    }

    if (type_.empty())
    {
      throw Error{
          ErrorCode::InvalidCommand,
          "room command type cannot be empty"};
    }

    if (type_.size() > max_type_size)
    {
      throw Error{
          ErrorCode::InvalidCommand,
          "room command type exceeds the maximum size of 128 characters"};
    }

    if (!is_valid_type(type_))
    {
      throw Error{
          ErrorCode::InvalidCommand,
          "room command type is invalid"};
    }
  }

  bool RoomCommand::is_valid_type(std::string_view value) noexcept
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
