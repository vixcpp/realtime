/**
 *
 * @file room_owner.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime room ownership descriptors.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/room_owner.hpp>

#include <limits>
#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  RoomOwner::RoomOwner(
      RoomId roomId,
      NodeId nodeId,
      RoomOwnerGeneration generation,
      Timestamp acquiredAt,
      std::optional<Timestamp> expiresAt,
      JsonObject metadata)
      : roomId_(std::move(roomId)),
        nodeId_(std::move(nodeId)),
        generation_(generation),
        status_(RoomOwnerStatus::Active),
        acquiredAt_(acquiredAt),
        renewedAt_(acquiredAt),
        expiresAt_(expiresAt),
        releasedAt_(),
        metadata_(std::move(metadata))
  {
    validate();
  }

  RoomOwner RoomOwner::leased(
      RoomId roomId,
      NodeId nodeId,
      RoomOwnerGeneration generation,
      Timestamp acquiredAt,
      std::chrono::milliseconds leaseDuration,
      JsonObject metadata)
  {
    if (leaseDuration.count() <= 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership lease duration must be positive"};
    }

    return RoomOwner{
        std::move(roomId),
        std::move(nodeId),
        generation,
        acquiredAt,
        acquiredAt + leaseDuration,
        std::move(metadata)};
  }

  const RoomId &RoomOwner::room_id() const noexcept
  {
    return roomId_;
  }

  const NodeId &RoomOwner::node_id() const noexcept
  {
    return nodeId_;
  }

  RoomOwnerGeneration
  RoomOwner::generation() const noexcept
  {
    return generation_;
  }

  RoomOwnerGeneration RoomOwner::next_generation() const
  {
    if (generation_ ==
        std::numeric_limits<RoomOwnerGeneration>::max())
    {
      throw Error{
          ErrorCode::InternalError,
          "room ownership generation overflow"};
    }

    return generation_ + 1;
  }

  RoomOwnerStatus RoomOwner::status() const noexcept
  {
    return status_;
  }

  Timestamp RoomOwner::acquired_at() const noexcept
  {
    return acquiredAt_;
  }

  Timestamp RoomOwner::renewed_at() const noexcept
  {
    return renewedAt_;
  }

  const std::optional<Timestamp> &
  RoomOwner::expires_at() const noexcept
  {
    return expiresAt_;
  }

  const std::optional<Timestamp> &
  RoomOwner::released_at() const noexcept
  {
    return releasedAt_;
  }

  bool RoomOwner::has_lease() const noexcept
  {
    return expiresAt_.has_value();
  }

  bool RoomOwner::expired(Timestamp now) const noexcept
  {
    if (status_ == RoomOwnerStatus::Released)
    {
      return true;
    }

    return expiresAt_.has_value() &&
           now >= *expiresAt_;
  }

  bool RoomOwner::active(Timestamp now) const noexcept
  {
    return status_ == RoomOwnerStatus::Active &&
           !expired(now);
  }

  bool RoomOwner::owned_by(
      const NodeId &nodeId,
      Timestamp now) const noexcept
  {
    return !nodeId.empty() &&
           nodeId_ == nodeId &&
           active(now);
  }

  bool RoomOwner::matches(
      const NodeId &nodeId,
      RoomOwnerGeneration generation) const noexcept
  {
    return !nodeId.empty() &&
           nodeId_ == nodeId &&
           generation_ == generation;
  }

  RoomOwner &RoomOwner::renew(
      Timestamp now,
      std::chrono::milliseconds leaseDuration)
  {
    if (leaseDuration.count() <= 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership lease duration must be positive"};
    }

    if (status_ != RoomOwnerStatus::Active)
    {
      throw Error{
          ErrorCode::RoomNotReady,
          "inactive room ownership cannot be renewed"};
    }

    if (now < acquiredAt_ ||
        now < renewedAt_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room ownership renewal timestamp moved backwards"};
    }

    if (expired(now))
    {
      throw Error{
          ErrorCode::RoomNotReady,
          "expired room ownership cannot be renewed"};
    }

    renewedAt_ = now;
    expiresAt_ = now + leaseDuration;

    return *this;
  }

  RoomOwner &RoomOwner::clear_expiration(
      Timestamp now)
  {
    if (status_ != RoomOwnerStatus::Active)
    {
      throw Error{
          ErrorCode::RoomNotReady,
          "inactive room ownership cannot become permanent"};
    }

    if (now < acquiredAt_ ||
        now < renewedAt_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room ownership update timestamp moved backwards"};
    }

    if (expired(now))
    {
      throw Error{
          ErrorCode::RoomNotReady,
          "expired room ownership cannot become permanent"};
    }

    renewedAt_ = now;
    expiresAt_.reset();

    return *this;
  }

  RoomOwner &RoomOwner::begin_release(
      Timestamp now)
  {
    if (status_ == RoomOwnerStatus::Released ||
        status_ == RoomOwnerStatus::Releasing)
    {
      return *this;
    }

    if (now < acquiredAt_ ||
        now < renewedAt_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room ownership release timestamp moved backwards"};
    }

    status_ = RoomOwnerStatus::Releasing;
    renewedAt_ = now;

    return *this;
  }

  RoomOwner &RoomOwner::release(Timestamp now)
  {
    if (status_ == RoomOwnerStatus::Released)
    {
      return *this;
    }

    if (now < acquiredAt_ ||
        now < renewedAt_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room ownership release timestamp moved backwards"};
    }

    status_ = RoomOwnerStatus::Released;
    renewedAt_ = now;
    releasedAt_ = now;

    return *this;
  }

  const JsonObject &
  RoomOwner::metadata() const noexcept
  {
    return metadata_;
  }

  RoomOwner &RoomOwner::set_metadata(
      JsonObject value)
  {
    metadata_ = std::move(value);
    return *this;
  }

  bool RoomOwner::is_valid() const noexcept
  {
    if (roomId_.empty() ||
        nodeId_.empty() ||
        generation_ == 0)
    {
      return false;
    }

    if (renewedAt_ < acquiredAt_)
    {
      return false;
    }

    if (expiresAt_.has_value() &&
        *expiresAt_ <= renewedAt_)
    {
      return false;
    }

    if (status_ == RoomOwnerStatus::Released)
    {
      if (!releasedAt_.has_value())
      {
        return false;
      }

      if (*releasedAt_ < acquiredAt_ ||
          *releasedAt_ < renewedAt_)
      {
        return false;
      }
    }
    else if (releasedAt_.has_value())
    {
      return false;
    }

    return true;
  }

  void RoomOwner::validate() const
  {
    if (roomId_.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership requires a room identifier"};
    }

    if (nodeId_.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership requires a node identifier"};
    }

    if (generation_ == 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership generation must be greater than zero"};
    }

    if (renewedAt_ < acquiredAt_)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room ownership renewal timestamp precedes acquisition"};
    }

    if (expiresAt_.has_value() &&
        *expiresAt_ <= renewedAt_)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership expiration must follow its latest renewal"};
    }

    if (status_ == RoomOwnerStatus::Released)
    {
      if (!releasedAt_.has_value())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "released room ownership requires a release timestamp"};
      }

      if (*releasedAt_ < acquiredAt_ ||
          *releasedAt_ < renewedAt_)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "room ownership release timestamp is inconsistent"};
      }
    }
    else if (releasedAt_.has_value())
    {
      throw Error{
          ErrorCode::CorruptedState,
          "active room ownership cannot contain a release timestamp"};
    }
  }

} // namespace vix::realtime
