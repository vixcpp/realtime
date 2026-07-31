/**
 *
 * @file room_directory.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime room ownership directory.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/room_directory.hpp>

#include <algorithm>
#include <limits>
#include <utility>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/room.hpp>

namespace vix::realtime
{
  namespace
  {
    /**
     * @brief Sort ownership descriptors by room identifier.
     */
    void sort_by_room_id(
        std::vector<RoomOwner> &owners)
    {
      std::sort(
          owners.begin(),
          owners.end(),
          [](const RoomOwner &left,
             const RoomOwner &right)
          {
            return left.room_id() <
                   right.room_id();
          });
    }

  } // namespace

  void RoomDirectory::validate_lease(
      const std::optional<std::chrono::milliseconds> &leaseDuration)
  {
    if (leaseDuration.has_value() &&
        leaseDuration->count() <= 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership lease duration must be positive"};
    }
  }

  RoomOwnerGeneration
  RoomDirectory::next_generation_locked(
      const RoomId &roomId)
  {
    const auto iterator =
        generations_.find(roomId);

    if (iterator == generations_.end())
    {
      generations_.emplace(roomId, 1);
      return 1;
    }

    if (iterator->second ==
        std::numeric_limits<RoomOwnerGeneration>::max())
    {
      throw Error{
          ErrorCode::InternalError,
          "room ownership generation overflow"};
    }

    ++iterator->second;
    return iterator->second;
  }

  RoomOwner &RoomDirectory::require_owner_locked(
      const RoomId &roomId,
      const NodeId &nodeId,
      RoomOwnerGeneration generation,
      Timestamp now,
      bool requireActive)
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "room ownership lookup requires a room identifier"};
    }

    if (nodeId.empty())
    {
      throw Error{
          ErrorCode::Unauthorized,
          "room ownership lookup requires a node identifier"};
    }

    if (generation == 0)
    {
      throw Error{
          ErrorCode::Unauthorized,
          "room ownership generation must be greater than zero"};
    }

    const auto iterator =
        owners_.find(roomId);

    if (iterator == owners_.end())
    {
      throw Error{
          ErrorCode::RoomNotFound,
          "room ownership claim was not found"};
    }

    RoomOwner &owner =
        iterator->second;

    if (!owner.matches(nodeId, generation))
    {
      throw Error{
          ErrorCode::Unauthorized,
          "room ownership claim does not match the current owner"};
    }

    if (owner.expired(now))
    {
      throw Error{
          ErrorCode::RoomNotReady,
          "room ownership claim has expired"};
    }

    if (requireActive &&
        !owner.active(now))
    {
      throw Error{
          ErrorCode::RoomNotReady,
          "room ownership claim is not active"};
    }

    return owner;
  }

  RoomOwner RoomDirectory::create_owner(
      RoomId roomId,
      NodeId nodeId,
      RoomOwnerGeneration generation,
      const std::optional<std::chrono::milliseconds> &leaseDuration,
      Timestamp now,
      JsonObject metadata)
  {
    if (leaseDuration)
    {
      return RoomOwner::leased(
          std::move(roomId),
          std::move(nodeId),
          generation,
          now,
          *leaseDuration,
          std::move(metadata));
    }

    return RoomOwner{
        std::move(roomId),
        std::move(nodeId),
        generation,
        now,
        std::nullopt,
        std::move(metadata)};
  }

  RoomOwner RoomDirectory::acquire(
      RoomId roomId,
      NodeId nodeId,
      std::optional<std::chrono::milliseconds> leaseDuration,
      Timestamp now,
      JsonObject metadata)
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership acquisition requires a room identifier"};
    }

    if (nodeId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership acquisition requires a node identifier"};
    }

    validate_lease(leaseDuration);

    std::lock_guard<std::mutex> lock{mutex_};

    const auto existingIterator =
        owners_.find(roomId);

    if (existingIterator != owners_.end())
    {
      const RoomOwner &existing =
          existingIterator->second;

      if (!existing.expired(now))
      {
        throw Error{
            ErrorCode::RoomAlreadyExists,
            "room already has an ownership claim"};
      }

      owners_.erase(existingIterator);
    }

    const RoomOwnerGeneration generation =
        next_generation_locked(roomId);

    RoomOwner owner =
        create_owner(
            std::move(roomId),
            std::move(nodeId),
            generation,
            leaseDuration,
            now,
            std::move(metadata));

    const RoomId key =
        owner.room_id();

    owners_.insert_or_assign(
        key,
        owner);

    return owner;
  }

  RoomOwner RoomDirectory::register_owner(
      RoomOwner owner,
      Timestamp now)
  {
    owner.validate();

    if (!owner.active(now))
    {
      throw Error{
          ErrorCode::RoomNotReady,
          "only active ownership claims can be registered"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const RoomId roomId =
        owner.room_id();

    const auto generationIterator =
        generations_.find(roomId);

    if (generationIterator != generations_.end() &&
        owner.generation() <=
            generationIterator->second)
    {
      throw Error{
          ErrorCode::RoomAlreadyExists,
          "room ownership generation is stale"};
    }

    const auto ownerIterator =
        owners_.find(roomId);

    if (ownerIterator != owners_.end() &&
        !ownerIterator->second.expired(now))
    {
      throw Error{
          ErrorCode::RoomAlreadyExists,
          "room already has a current ownership claim"};
    }

    generations_.insert_or_assign(
        roomId,
        owner.generation());

    owners_.insert_or_assign(
        roomId,
        owner);

    return owner;
  }

  RoomOwner RoomDirectory::renew(
      const RoomId &roomId,
      const NodeId &nodeId,
      RoomOwnerGeneration generation,
      std::chrono::milliseconds leaseDuration,
      Timestamp now)
  {
    if (leaseDuration.count() <= 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership lease duration must be positive"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    RoomOwner &owner =
        require_owner_locked(
            roomId,
            nodeId,
            generation,
            now,
            true);

    owner.renew(
        now,
        leaseDuration);

    return owner;
  }

  RoomOwner RoomDirectory::make_permanent(
      const RoomId &roomId,
      const NodeId &nodeId,
      RoomOwnerGeneration generation,
      Timestamp now)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    RoomOwner &owner =
        require_owner_locked(
            roomId,
            nodeId,
            generation,
            now,
            true);

    owner.clear_expiration(now);

    return owner;
  }

  RoomOwner RoomDirectory::transfer(
      const RoomId &roomId,
      const NodeId &currentNodeId,
      RoomOwnerGeneration currentGeneration,
      NodeId nextNodeId,
      std::optional<std::chrono::milliseconds> leaseDuration,
      Timestamp now,
      JsonObject metadata)
  {
    if (nextNodeId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership transfer requires a destination node"};
    }

    validate_lease(leaseDuration);

    std::lock_guard<std::mutex> lock{mutex_};

    static_cast<void>(
        require_owner_locked(
            roomId,
            currentNodeId,
            currentGeneration,
            now,
            true));

    const RoomOwnerGeneration nextGeneration =
        next_generation_locked(roomId);

    RoomOwner nextOwner =
        create_owner(
            roomId,
            std::move(nextNodeId),
            nextGeneration,
            leaseDuration,
            now,
            std::move(metadata));

    owners_.insert_or_assign(
        roomId,
        nextOwner);

    return nextOwner;
  }

  RoomOwner RoomDirectory::begin_release(
      const RoomId &roomId,
      const NodeId &nodeId,
      RoomOwnerGeneration generation,
      Timestamp now)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    RoomOwner &owner =
        require_owner_locked(
            roomId,
            nodeId,
            generation,
            now,
            true);

    owner.begin_release(now);

    return owner;
  }

  RoomOwner RoomDirectory::release(
      const RoomId &roomId,
      const NodeId &nodeId,
      RoomOwnerGeneration generation,
      Timestamp now)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    RoomOwner &storedOwner =
        require_owner_locked(
            roomId,
            nodeId,
            generation,
            now,
            false);

    RoomOwner releasedOwner =
        storedOwner;

    releasedOwner.release(now);

    owners_.erase(roomId);

    return releasedOwner;
  }

  std::optional<RoomOwner> RoomDirectory::resolve(
      const RoomId &roomId,
      Timestamp now) const
  {
    if (roomId.empty())
    {
      return std::nullopt;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        owners_.find(roomId);

    if (iterator == owners_.end() ||
        !iterator->second.active(now))
    {
      return std::nullopt;
    }

    return iterator->second;
  }

  std::optional<RoomOwner> RoomDirectory::inspect(
      const RoomId &roomId) const
  {
    if (roomId.empty())
    {
      return std::nullopt;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        owners_.find(roomId);

    if (iterator == owners_.end())
    {
      return std::nullopt;
    }

    return iterator->second;
  }

  bool RoomDirectory::owns(
      const RoomId &roomId,
      const NodeId &nodeId,
      Timestamp now) const
  {
    if (roomId.empty() ||
        nodeId.empty())
    {
      return false;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        owners_.find(roomId);

    return iterator != owners_.end() &&
           iterator->second.owned_by(
               nodeId,
               now);
  }

  bool RoomDirectory::matches(
      const RoomId &roomId,
      const NodeId &nodeId,
      RoomOwnerGeneration generation,
      Timestamp now) const
  {
    if (roomId.empty() ||
        nodeId.empty() ||
        generation == 0)
    {
      return false;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        owners_.find(roomId);

    return iterator != owners_.end() &&
           iterator->second.active(now) &&
           iterator->second.matches(
               nodeId,
               generation);
  }

  std::vector<RoomOwner>
  RoomDirectory::active_owners(
      Timestamp now) const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    std::vector<RoomOwner> result;
    result.reserve(owners_.size());

    for (const auto &[roomId, owner] : owners_)
    {
      static_cast<void>(roomId);

      if (owner.active(now))
      {
        result.push_back(owner);
      }
    }

    sort_by_room_id(result);
    return result;
  }

  std::vector<RoomOwner>
  RoomDirectory::owned_by(
      const NodeId &nodeId,
      Timestamp now) const
  {
    if (nodeId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room ownership listing requires a node identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    std::vector<RoomOwner> result;

    for (const auto &[roomId, owner] : owners_)
    {
      static_cast<void>(roomId);

      if (owner.owned_by(nodeId, now))
      {
        result.push_back(owner);
      }
    }

    sort_by_room_id(result);
    return result;
  }

  RoomOwnerGeneration
  RoomDirectory::latest_generation(
      const RoomId &roomId) const
  {
    if (roomId.empty())
    {
      return 0;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        generations_.find(roomId);

    if (iterator == generations_.end())
    {
      return 0;
    }

    return iterator->second;
  }

  bool RoomDirectory::register_room(
      const RoomPtr &room)
  {
    if (!room)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "room directory cannot register a null room"};
    }

    return register_room(
        room->id(),
        room);
  }

  bool RoomDirectory::register_room(
      const RoomId &roomId,
      const RoomPtr &room)
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room directory registration requires a room identifier"};
    }

    if (!room)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "room directory cannot register a null room"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    return rooms_.emplace(roomId, room).second;
  }

  RoomPtr RoomDirectory::find(
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

  RoomPtr RoomDirectory::find_room(
      const RoomId &roomId) const
  {
    return find(roomId);
  }

  bool RoomDirectory::remove(
      const RoomId &roomId)
  {
    if (roomId.empty())
    {
      return false;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    return rooms_.erase(roomId) != 0;
  }

  bool RoomDirectory::unregister_room(
      const RoomId &roomId)
  {
    return remove(roomId);
  }

  bool RoomDirectory::contains(
      const RoomId &roomId) const
  {
    return find(roomId) != nullptr;
  }

  bool RoomDirectory::contains_room(
      const RoomId &roomId) const
  {
    return contains(roomId);
  }

  std::size_t RoomDirectory::room_count() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return rooms_.size();
  }

  std::size_t RoomDirectory::size() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return rooms_.empty()
               ? owners_.size()
               : rooms_.size();
  }

  std::size_t RoomDirectory::active_count(
      Timestamp now) const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    std::size_t count = 0;

    for (const auto &[roomId, owner] : owners_)
    {
      static_cast<void>(roomId);

      if (owner.active(now))
      {
        ++count;
      }
    }

    return count;
  }

  std::size_t RoomDirectory::prune_expired(
      Timestamp now)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    std::size_t removed = 0;

    for (auto iterator = owners_.begin();
         iterator != owners_.end();)
    {
      if (iterator->second.expired(now))
      {
        iterator = owners_.erase(iterator);
        ++removed;
      }
      else
      {
        ++iterator;
      }
    }

    return removed;
  }

  bool RoomDirectory::clear_room(
      const RoomId &roomId)
  {
    if (roomId.empty())
    {
      return false;
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const bool removedOwner =
        owners_.erase(roomId) != 0;

    const bool removedRoom =
        rooms_.erase(roomId) != 0;

    return removedOwner || removedRoom;
  }

  void RoomDirectory::clear()
  {
    std::lock_guard<std::mutex> lock{mutex_};

    owners_.clear();
    generations_.clear();
    rooms_.clear();
  }

} // namespace vix::realtime
