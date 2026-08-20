/** @file room_runtime.hpp @brief Private Room runtime state. */

#ifndef VIX_REALTIME_SRC_INTERNAL_ROOM_RUNTIME_HPP
#define VIX_REALTIME_SRC_INTERNAL_ROOM_RUNTIME_HPP

#include <cstddef>

#include <vix/realtime/internal/command_queue.hpp>
#include <vix/realtime/internal/snapshot_policy.hpp>

namespace vix::realtime::internal
{
  struct RoomRuntime
  {
    RoomRuntime(
        std::size_t maxPendingCommands,
        std::size_t snapshotEveryEvents,
        std::size_t snapshotsToKeep,
        bool snapshotOnRoomClose)
        : commandQueue(maxPendingCommands),
          snapshotPolicy(
              snapshotEveryEvents,
              snapshotsToKeep,
              snapshotOnRoomClose)
    {
    }

    CommandQueue commandQueue;
    SnapshotPolicy snapshotPolicy;
  };
} // namespace vix::realtime::internal

#endif // VIX_REALTIME_SRC_INTERNAL_ROOM_RUNTIME_HPP
