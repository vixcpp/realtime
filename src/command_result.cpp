/**
 *
 * @file command_result.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime command results.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/command_result.hpp>

#include <utility>

namespace vix::realtime
{
  CommandResult::CommandResult(
      CommandStatus status,
      std::vector<RoomEvent> events,
      std::optional<ErrorCode> errorCode,
      std::string message)
      : status_(status),
        events_(std::move(events)),
        errorCode_(errorCode),
        message_(std::move(message)),
        metadata_()
  {
    validate();
  }

  CommandResult CommandResult::accepted()
  {
    return CommandResult{
        CommandStatus::Accepted};
  }

  CommandResult CommandResult::accepted(RoomEvent event)
  {
    std::vector<RoomEvent> events;
    events.reserve(1);
    events.push_back(std::move(event));

    return CommandResult{
        CommandStatus::Accepted,
        std::move(events)};
  }

  CommandResult CommandResult::accepted(
      std::vector<RoomEvent> events)
  {
    return CommandResult{
        CommandStatus::Accepted,
        std::move(events)};
  }

  CommandResult CommandResult::rejected(
      ErrorCode code,
      std::string message)
  {
    if (code == ErrorCode::None)
    {
      throw Error{
          ErrorCode::InternalError,
          "rejected command result requires a non-empty error code"};
    }

    return CommandResult{
        CommandStatus::Rejected,
        {},
        code,
        std::move(message)};
  }

  CommandResult CommandResult::ignored(std::string message)
  {
    return CommandResult{
        CommandStatus::Ignored,
        {},
        std::nullopt,
        std::move(message)};
  }

  CommandStatus CommandResult::status() const noexcept
  {
    return status_;
  }

  bool CommandResult::is_accepted() const noexcept
  {
    return status_ == CommandStatus::Accepted;
  }

  bool CommandResult::is_rejected() const noexcept
  {
    return status_ == CommandStatus::Rejected;
  }

  bool CommandResult::is_ignored() const noexcept
  {
    return status_ == CommandStatus::Ignored;
  }

  bool CommandResult::has_events() const noexcept
  {
    return !events_.empty();
  }

  std::size_t CommandResult::event_count() const noexcept
  {
    return events_.size();
  }

  const std::vector<RoomEvent> &
  CommandResult::events() const noexcept
  {
    return events_;
  }

  std::vector<RoomEvent> &CommandResult::events() noexcept
  {
    return events_;
  }

  const std::optional<ErrorCode> &
  CommandResult::error_code() const noexcept
  {
    return errorCode_;
  }

  const std::string &CommandResult::message() const noexcept
  {
    return message_;
  }

  const JsonObject &CommandResult::metadata() const noexcept
  {
    return metadata_;
  }

  CommandResult &CommandResult::add_event(RoomEvent event)
  {
    if (!is_accepted())
    {
      throw Error{
          ErrorCode::InternalError,
          "events can only be added to an accepted command result"};
    }

    event.validate();
    events_.push_back(std::move(event));

    return *this;
  }

  CommandResult &CommandResult::set_message(std::string value)
  {
    message_ = std::move(value);
    return *this;
  }

  CommandResult &CommandResult::set_metadata(JsonObject value)
  {
    metadata_ = std::move(value);
    return *this;
  }

  bool CommandResult::is_valid() const noexcept
  {
    switch (status_)
    {
    case CommandStatus::Accepted:
      if (errorCode_.has_value())
      {
        return false;
      }

      for (const auto &event : events_)
      {
        if (!event.is_valid())
        {
          return false;
        }
      }

      return true;

    case CommandStatus::Rejected:
      return events_.empty() &&
             errorCode_.has_value() &&
             *errorCode_ != ErrorCode::None;

    case CommandStatus::Ignored:
      return events_.empty() &&
             !errorCode_.has_value();
    }

    return false;
  }

  void CommandResult::validate() const
  {
    switch (status_)
    {
    case CommandStatus::Accepted:
      if (errorCode_.has_value())
      {
        throw Error{
            ErrorCode::InternalError,
            "accepted command result cannot contain an error code"};
      }

      for (const auto &event : events_)
      {
        event.validate();
      }

      return;

    case CommandStatus::Rejected:
      if (!events_.empty())
      {
        throw Error{
            ErrorCode::InternalError,
            "rejected command result cannot contain events"};
      }

      if (!errorCode_.has_value() ||
          *errorCode_ == ErrorCode::None)
      {
        throw Error{
            ErrorCode::InternalError,
            "rejected command result requires a non-empty error code"};
      }

      return;

    case CommandStatus::Ignored:
      if (!events_.empty())
      {
        throw Error{
            ErrorCode::InternalError,
            "ignored command result cannot contain events"};
      }

      if (errorCode_.has_value())
      {
        throw Error{
            ErrorCode::InternalError,
            "ignored command result cannot contain an error code"};
      }

      return;
    }

    throw Error{
        ErrorCode::InternalError,
        "command result contains an unknown status"};
  }

} // namespace vix::realtime
