/**
 *
 * @file room_snapshot.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime room snapshots.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/room_snapshot.hpp>

#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  RoomSnapshot::RoomSnapshot(
      RoomId roomId,
      RoomVersion roomVersion,
      EventId lastEventId,
      JsonObject state,
      SchemaVersion schemaVersion)
      : roomId_(std::move(roomId)),
        roomVersion_(roomVersion),
        lastEventId_(lastEventId),
        state_(std::move(state)),
        schemaVersion_(schemaVersion),
        createdAt_(SystemClock::now()),
        checksum_(),
        metadata_()
  {
    validate();
  }

  const RoomId &RoomSnapshot::room_id() const noexcept
  {
    return roomId_;
  }

  RoomVersion RoomSnapshot::room_version() const noexcept
  {
    return roomVersion_;
  }

  EventId RoomSnapshot::last_event_id() const noexcept
  {
    return lastEventId_;
  }

  const JsonObject &RoomSnapshot::state() const noexcept
  {
    return state_;
  }

  JsonObject &RoomSnapshot::state() noexcept
  {
    return state_;
  }

  SchemaVersion RoomSnapshot::schema_version() const noexcept
  {
    return schemaVersion_;
  }

  Timestamp RoomSnapshot::created_at() const noexcept
  {
    return createdAt_;
  }

  const std::optional<std::string> &
  RoomSnapshot::checksum() const noexcept
  {
    return checksum_;
  }

  const JsonObject &RoomSnapshot::metadata() const noexcept
  {
    return metadata_;
  }

  RoomSnapshot &RoomSnapshot::set_room_version(
      RoomVersion value) noexcept
  {
    roomVersion_ = value;
    return *this;
  }

  RoomSnapshot &RoomSnapshot::set_last_event_id(
      EventId value) noexcept
  {
    lastEventId_ = value;
    return *this;
  }

  RoomSnapshot &RoomSnapshot::set_schema_version(
      SchemaVersion value)
  {
    if (value == 0)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room snapshot schema version cannot be zero"};
    }

    schemaVersion_ = value;
    return *this;
  }

  RoomSnapshot &RoomSnapshot::set_created_at(
      Timestamp value) noexcept
  {
    createdAt_ = value;
    return *this;
  }

  RoomSnapshot &RoomSnapshot::set_checksum(std::string value)
  {
    if (value.empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room snapshot checksum cannot be empty"};
    }

    checksum_ = std::move(value);
    return *this;
  }

  RoomSnapshot &RoomSnapshot::clear_checksum() noexcept
  {
    checksum_.reset();
    return *this;
  }

  RoomSnapshot &RoomSnapshot::set_metadata(JsonObject value)
  {
    metadata_ = std::move(value);
    return *this;
  }

  bool RoomSnapshot::is_valid() const noexcept
  {
    if (roomId_.empty() || schemaVersion_ == 0)
    {
      return false;
    }

    if (roomVersion_.is_initial() && !lastEventId_.empty())
    {
      return false;
    }

    if (!roomVersion_.is_initial() && lastEventId_.empty())
    {
      return false;
    }

    if (checksum_.has_value() && checksum_->empty())
    {
      return false;
    }

    return true;
  }

  void RoomSnapshot::validate() const
  {
    if (roomId_.empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room snapshot requires a room identifier"};
    }

    if (schemaVersion_ == 0)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room snapshot schema version cannot be zero"};
    }

    if (roomVersion_.is_initial() && !lastEventId_.empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "initial room snapshot cannot reference a persisted event"};
    }

    if (!roomVersion_.is_initial() && lastEventId_.empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "versioned room snapshot requires a last event identifier"};
    }

    if (checksum_.has_value() && checksum_->empty())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room snapshot checksum cannot be empty"};
    }
  }

} // namespace vix::realtime
