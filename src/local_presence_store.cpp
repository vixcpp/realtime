/**
 *
 * @file local_presence_store.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime process-local presence store.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/local_presence_store.hpp>

#include <algorithm>
#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  void LocalPresenceStore::validate_key(
      const RoomId &roomId,
      const SessionId &sessionId)
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "presence lookup requires a room identifier"};
    }

    if (sessionId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "presence lookup requires a session identifier"};
    }
  }

  Presence &LocalPresenceStore::require_locked(
      const RoomId &roomId,
      const SessionId &sessionId)
  {
    const auto roomIterator =
        rooms_.find(roomId);

    if (roomIterator == rooms_.end())
    {
      throw Error{
          ErrorCode::MembershipNotFound,
          "room presence was not found"};
    }

    const auto presenceIterator =
        roomIterator->second.find(sessionId);

    if (presenceIterator ==
        roomIterator->second.end())
    {
      throw Error{
          ErrorCode::MembershipNotFound,
          "session presence was not found in the room"};
    }

    return presenceIterator->second;
  }

  Presence LocalPresenceStore::upsert(
      Presence presence)
  {
    presence.validate();

    std::lock_guard<std::mutex> lock{mutex_};

    SessionPresenceMap &roomPresences =
        rooms_[presence.room_id()];

    const auto existingIterator =
        roomPresences.find(
            presence.session_id());

    if (existingIterator != roomPresences.end())
    {
      const Presence &existing =
          existingIterator->second;

      if (presence.joined_at() <
              existing.joined_at() ||
          presence.last_seen_at() <
              existing.last_seen_at())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "presence replacement moved membership timestamps backwards"};
      }

      existingIterator->second =
          std::move(presence);

      return existingIterator->second;
    }

    const SessionId sessionId =
        presence.session_id();

    const auto [iterator, inserted] =
        roomPresences.emplace(
            sessionId,
            std::move(presence));

    if (inserted)
    {
      ++count_;
    }

    return iterator->second;
  }

  std::optional<Presence>
  LocalPresenceStore::find(
      const RoomId &roomId,
      const SessionId &sessionId) const
  {
    validate_key(roomId, sessionId);

    std::lock_guard<std::mutex> lock{mutex_};

    const auto roomIterator =
        rooms_.find(roomId);

    if (roomIterator == rooms_.end())
    {
      return std::nullopt;
    }

    const auto presenceIterator =
        roomIterator->second.find(sessionId);

    if (presenceIterator ==
        roomIterator->second.end())
    {
      return std::nullopt;
    }

    return presenceIterator->second;
  }

  Presence LocalPresenceStore::touch(
      const RoomId &roomId,
      const SessionId &sessionId,
      Timestamp now)
  {
    validate_key(roomId, sessionId);

    std::lock_guard<std::mutex> lock{mutex_};

    Presence &presence =
        require_locked(roomId, sessionId);

    presence.touch(now);
    return presence;
  }

  Presence LocalPresenceStore::mark_present(
      const RoomId &roomId,
      const SessionId &sessionId,
      ConnectionId connectionId,
      std::optional<NodeId> nodeId,
      Timestamp now)
  {
    validate_key(roomId, sessionId);

    std::lock_guard<std::mutex> lock{mutex_};

    Presence &presence =
        require_locked(roomId, sessionId);

    presence.mark_present(
        std::move(connectionId),
        std::move(nodeId),
        now);

    return presence;
  }

  Presence LocalPresenceStore::mark_detached(
      const RoomId &roomId,
      const SessionId &sessionId,
      Timestamp now)
  {
    validate_key(roomId, sessionId);

    std::lock_guard<std::mutex> lock{mutex_};

    Presence &presence =
        require_locked(roomId, sessionId);

    presence.mark_detached(now);
    return presence;
  }

  Presence LocalPresenceStore::mark_left(
      const RoomId &roomId,
      const SessionId &sessionId,
      Timestamp now)
  {
    validate_key(roomId, sessionId);

    std::lock_guard<std::mutex> lock{mutex_};

    Presence &presence =
        require_locked(roomId, sessionId);

    presence.mark_left(now);
    return presence;
  }

  std::vector<Presence>
  LocalPresenceStore::list_room(
      const RoomId &roomId) const
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room presence listing requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto roomIterator =
        rooms_.find(roomId);

    if (roomIterator == rooms_.end())
    {
      return {};
    }

    std::vector<Presence> result;
    result.reserve(
        roomIterator->second.size());

    for (const auto &[sessionId, presence] :
         roomIterator->second)
    {
      static_cast<void>(sessionId);
      if (presence.status() !=
          PresenceStatus::Left)
      {
        result.push_back(presence);
      }
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const Presence &left,
           const Presence &right)
        {
          return left.session_id() <
                 right.session_id();
        });

    return result;
  }

  std::vector<Presence>
  LocalPresenceStore::list_session(
      const SessionId &sessionId) const
  {
    if (sessionId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "session presence listing requires a session identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    std::vector<Presence> result;

    for (const auto &[roomId, presences] : rooms_)
    {
      static_cast<void>(roomId);

      const auto presenceIterator =
          presences.find(sessionId);

      if (presenceIterator != presences.end())
      {
        if (presenceIterator->second.status() !=
            PresenceStatus::Left)
        {
          result.push_back(
              presenceIterator->second);
        }
      }
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const Presence &left,
           const Presence &right)
        {
          return left.room_id() <
                 right.room_id();
        });

    return result;
  }

  std::optional<Presence>
  LocalPresenceStore::erase(
      const RoomId &roomId,
      const SessionId &sessionId)
  {
    validate_key(roomId, sessionId);

    std::lock_guard<std::mutex> lock{mutex_};

    const auto roomIterator =
        rooms_.find(roomId);

    if (roomIterator == rooms_.end())
    {
      return std::nullopt;
    }

    auto presenceIterator =
        roomIterator->second.find(sessionId);

    if (presenceIterator ==
        roomIterator->second.end())
    {
      return std::nullopt;
    }

    Presence removed =
        std::move(presenceIterator->second);

    roomIterator->second.erase(
        presenceIterator);

    --count_;

    if (roomIterator->second.empty())
    {
      rooms_.erase(roomIterator);
    }

    return removed;
  }

  std::size_t LocalPresenceStore::prune_stale(
      Timestamp now,
      std::chrono::milliseconds timeout)
  {
    if (timeout.count() < 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "presence timeout cannot be negative"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    std::size_t removed = 0;

    for (auto roomIterator = rooms_.begin();
         roomIterator != rooms_.end();)
    {
      SessionPresenceMap &presences =
          roomIterator->second;

      for (auto presenceIterator =
               presences.begin();
           presenceIterator != presences.end();)
      {
        if (presenceIterator->second.stale(
                now,
                timeout))
        {
          presenceIterator =
              presences.erase(
                  presenceIterator);

          ++removed;
          --count_;
        }
        else
        {
          ++presenceIterator;
        }
      }

      if (presences.empty())
      {
        roomIterator =
            rooms_.erase(roomIterator);
      }
      else
      {
        ++roomIterator;
      }
    }

    return removed;
  }

  std::size_t LocalPresenceStore::count_room(
      const RoomId &roomId) const
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "room presence count requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        rooms_.find(roomId);

    if (iterator == rooms_.end())
    {
      return 0;
    }

    return static_cast<std::size_t>(
        std::count_if(
            iterator->second.begin(),
            iterator->second.end(),
            [](const auto &entry)
            {
              return entry.second.status() !=
                     PresenceStatus::Left;
            }));
  }

  std::size_t LocalPresenceStore::count() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return count_;
  }

  std::size_t LocalPresenceStore::clear_room(
      const RoomId &roomId)
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "presence removal requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        rooms_.find(roomId);

    if (iterator == rooms_.end())
    {
      return 0;
    }

    const std::size_t removed =
        iterator->second.size();

    rooms_.erase(iterator);
    count_ -= removed;

    return removed;
  }

  std::size_t LocalPresenceStore::clear_session(
      const SessionId &sessionId)
  {
    if (sessionId.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "presence removal requires a session identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    std::size_t removed = 0;

    for (auto roomIterator = rooms_.begin();
         roomIterator != rooms_.end();)
    {
      removed +=
          roomIterator->second.erase(
              sessionId);

      if (roomIterator->second.empty())
      {
        roomIterator =
            rooms_.erase(roomIterator);
      }
      else
      {
        ++roomIterator;
      }
    }

    count_ -= removed;
    return removed;
  }

  void LocalPresenceStore::clear()
  {
    std::lock_guard<std::mutex> lock{mutex_};

    rooms_.clear();
    count_ = 0;
  }

  std::size_t
  LocalPresenceStore::room_count() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return rooms_.size();
  }

} // namespace vix::realtime
