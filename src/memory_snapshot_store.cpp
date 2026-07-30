/**
 *
 * @file memory_snapshot_store.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime in-memory snapshot store.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/memory_snapshot_store.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  void MemorySnapshotStore::validate_save(
      const RoomSnapshot &snapshot,
      const SnapshotStream &stream)
  {
    snapshot.validate();

    if (stream.empty())
    {
      return;
    }

    const RoomSnapshot &latest =
        stream.back();

    if (snapshot.room_id() != latest.room_id())
    {
      throw Error{
          ErrorCode::SnapshotStoreFailure,
          "snapshot stream cannot contain multiple room identifiers"};
    }

    if (snapshot.room_version() <
        latest.room_version())
    {
      throw Error{
          ErrorCode::SnapshotStoreFailure,
          "cannot save a snapshot older than the latest stored version"};
    }

    if (snapshot.room_version() ==
        latest.room_version())
    {
      if (snapshot.last_event_id() !=
          latest.last_event_id())
      {
        throw Error{
            ErrorCode::SnapshotStoreFailure,
            "snapshots at the same room version must reference the same event"};
      }

      return;
    }

    if (!latest.last_event_id().empty() &&
        snapshot.last_event_id() <=
            latest.last_event_id())
    {
      throw Error{
          ErrorCode::SnapshotStoreFailure,
          "newer snapshot must advance the persisted event position"};
    }
  }

  RoomSnapshot MemorySnapshotStore::save(
      RoomSnapshot snapshot)
  {
    snapshot.validate();

    std::lock_guard<std::mutex> lock{mutex_};

    SnapshotStream &stream =
        streams_[snapshot.room_id()];

    validate_save(snapshot, stream);

    if (!stream.empty() &&
        stream.back().room_version() ==
            snapshot.room_version())
    {
      stream.back() = snapshot;
      return snapshot;
    }

    stream.push_back(snapshot);
    return snapshot;
  }

  std::optional<RoomSnapshot>
  MemorySnapshotStore::load_latest(
      const RoomId &roomId) const
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::SnapshotStoreFailure,
          "latest snapshot lookup requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        streams_.find(roomId);

    if (iterator == streams_.end() ||
        iterator->second.empty())
    {
      return std::nullopt;
    }

    return iterator->second.back();
  }

  std::optional<RoomSnapshot>
  MemorySnapshotStore::load_at_or_before(
      const RoomId &roomId,
      RoomVersion version) const
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::SnapshotStoreFailure,
          "historical snapshot lookup requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto streamIterator =
        streams_.find(roomId);

    if (streamIterator == streams_.end() ||
        streamIterator->second.empty())
    {
      return std::nullopt;
    }

    const SnapshotStream &stream =
        streamIterator->second;

    const auto upper =
        std::upper_bound(
            stream.begin(),
            stream.end(),
            version,
            [](RoomVersion requested,
               const RoomSnapshot &snapshot)
            {
              return requested <
                     snapshot.room_version();
            });

    if (upper == stream.begin())
    {
      return std::nullopt;
    }

    return *std::prev(upper);
  }

  std::vector<RoomSnapshot>
  MemorySnapshotStore::load_recent(
      const RoomId &roomId,
      std::size_t limit) const
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::SnapshotStoreFailure,
          "recent snapshot lookup requires a room identifier"};
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

    const SnapshotStream &stream =
        streamIterator->second;

    const std::size_t resultSize =
        std::min(limit, stream.size());

    std::vector<RoomSnapshot> result;
    result.reserve(resultSize);

    auto iterator = stream.rbegin();

    for (std::size_t index = 0;
         index < resultSize;
         ++index, ++iterator)
    {
      result.push_back(*iterator);
    }

    return result;
  }

  std::size_t MemorySnapshotStore::count(
      const RoomId &roomId) const
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::SnapshotStoreFailure,
          "snapshot count requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        streams_.find(roomId);

    if (iterator == streams_.end())
    {
      return 0;
    }

    return iterator->second.size();
  }

  std::size_t MemorySnapshotStore::prune(
      const RoomId &roomId,
      std::size_t keep)
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::SnapshotStoreFailure,
          "snapshot pruning requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const auto iterator =
        streams_.find(roomId);

    if (iterator == streams_.end())
    {
      return 0;
    }

    SnapshotStream &stream =
        iterator->second;

    if (stream.size() <= keep)
    {
      return 0;
    }

    const std::size_t removeCount =
        stream.size() - keep;

    if (keep == 0)
    {
      streams_.erase(iterator);
      return removeCount;
    }

    stream.erase(
        stream.begin(),
        stream.begin() +
            static_cast<std::ptrdiff_t>(
                removeCount));

    return removeCount;
  }

  bool MemorySnapshotStore::clear_room(
      const RoomId &roomId)
  {
    if (roomId.empty())
    {
      throw Error{
          ErrorCode::SnapshotStoreFailure,
          "snapshot stream removal requires a room identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    return streams_.erase(roomId) != 0;
  }

  void MemorySnapshotStore::clear()
  {
    std::lock_guard<std::mutex> lock{mutex_};

    streams_.clear();
  }

  std::size_t
  MemorySnapshotStore::room_count() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return streams_.size();
  }

} // namespace vix::realtime
