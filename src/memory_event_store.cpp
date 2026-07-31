/**
 *
 * @file memory_event_store.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime in-memory event store.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/memory_event_store.hpp>

#include <algorithm>
#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  void MemoryEventStore::validate_append(
      const RoomEvent &event,
      const Stream &stream)
  {
    event.validate();

    if (!event.event_id().empty())
    {
      throw Error{
          ErrorCode::EventStoreFailure,
          "event store cannot append an event that already has an event identifier"};
    }

    const RoomVersion expectedVersion =
        stream.lastRoomVersion.next();

    if (event.room_version() != expectedVersion)
    {
      throw Error{
          ErrorCode::EventStoreFailure,
          "room event version is not contiguous with the persisted stream"};
    }
  }

  RoomEvent MemoryEventStore::append(
      RoomEvent event)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    Stream &stream =
        streams_[event.room_id()];

    validate_append(event, stream);

    const EventId eventId =
        stream.lastEventId.next();

    event.set_event_id(eventId);

    stream.events.push_back(event);
    stream.lastEventId = eventId;
    stream.lastRoomVersion =
        event.room_version();

    return event;
  }

  std::vector<RoomEvent>
  MemoryEventStore::append_batch(
      std::vector<RoomEvent> events)
  {
    if (events.empty())
    {
      return {};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const RoomId roomId =
        events.front().room_id();

    const auto streamIterator =
        streams_.find(roomId);

    Stream stagedStream;

    if (streamIterator != streams_.end())
    {
      stagedStream = streamIterator->second;
    }

    std::vector<RoomEvent> persisted;
    persisted.reserve(events.size());

    for (auto &event : events)
    {
      if (event.room_id() != roomId)
      {
        throw Error{
            ErrorCode::EventStoreFailure,
            "event batch cannot contain multiple room identifiers"};
      }

      validate_append(event, stagedStream);

      const EventId eventId =
          stagedStream.lastEventId.next();

      event.set_event_id(eventId);

      stagedStream.events.push_back(event);
      stagedStream.lastEventId = eventId;
      stagedStream.lastRoomVersion =
          event.room_version();

      persisted.push_back(std::move(event));
    }

    streams_.insert_or_assign(
        roomId,
        std::move(stagedStream));

    return persisted;
  }

  std::vector<RoomEvent>
  MemoryEventStore::load_after(
      const RoomId &roomId,
      EventId after,
      std::size_t limit) const
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::EventStoreFailure,
          "event replay requires a room identifier"};
    }

    if (limit == 0)
    {
      return {};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto streamIterator =
        streams_.find(roomId);

    if (streamIterator == streams_.end())
    {
      return {};
    }

    const Stream &stream =
        streamIterator->second;

    if (after > stream.lastEventId)
    {
      return {};
    }

    const auto first =
        std::upper_bound(
            stream.events.begin(),
            stream.events.end(),
            after,
            [](EventId cursor, const RoomEvent &event)
            {
              return cursor < event.event_id();
            });

    const auto available =
        static_cast<std::size_t>(
            std::distance(first, stream.events.end()));

    const std::size_t count =
        std::min(limit, available);

    std::vector<RoomEvent> result;
    result.reserve(count);

    auto iterator = first;

    for (std::size_t index = 0;
         index < count;
         ++index, ++iterator)
    {
      result.push_back(*iterator);
    }

    return result;
  }

  EventId MemoryEventStore::latest_event_id(
      const RoomId &roomId) const
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::EventStoreFailure,
          "latest event lookup requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        streams_.find(roomId);

    if (iterator == streams_.end())
    {
      return EventId{};
    }

    return iterator->second.lastEventId;
  }

  std::size_t MemoryEventStore::count(
      const RoomId &roomId) const
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::EventStoreFailure,
          "event count requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        streams_.find(roomId);

    if (iterator == streams_.end())
    {
      return 0;
    }

    return iterator->second.events.size();
  }

  bool MemoryEventStore::clear_room(
      const RoomId &roomId)
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::EventStoreFailure,
          "event stream removal requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    return streams_.erase(roomId) != 0;
  }

  void MemoryEventStore::clear()
  {
    std::lock_guard<std::mutex> lock{mutex_};

    streams_.clear();
  }

  std::size_t
  MemoryEventStore::room_count() const noexcept
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return streams_.size();
  }

} // namespace vix::realtime
