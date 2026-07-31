#include <vix/realtime/config.hpp>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  Config Config::from_core(
      const vix::config::Config &)
  {
    return {};
  }

  void Config::validate() const
  {
    if (maxActiveRooms == 0 ||
        maxSessions == 0 ||
        maxSessionsPerRoom == 0 ||
        maxRoomsPerSession == 0 ||
        maxPendingCommandsPerRoom == 0 ||
        maxPayloadSize == 0 ||
        maxReplayEvents == 0 ||
        maxReplayBytes == 0 ||
        maxResumeRooms == 0 ||
        snapshotEveryEvents == 0 ||
        snapshotsToKeep == 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "realtime limits must be greater than zero"};
    }

    if (commandTimeout.count() < 0 ||
        roomOpenTimeout.count() < 0 ||
        sessionResumeWindow.count() < 0 ||
        presenceHeartbeatInterval.count() < 0 ||
        presenceTimeout.count() < 0 ||
        replayTimeout.count() < 0 ||
        roomIdleTimeout.count() < 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "realtime durations cannot be negative"};
    }

    if (enablePresence &&
        presenceTimeout < presenceHeartbeatInterval)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "presence timeout must be greater than or equal to heartbeat interval"};
    }
  }
} // namespace vix::realtime
