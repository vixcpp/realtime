#include <vix/realtime/config.hpp>

#include <chrono>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  Config Config::from_core(
      const vix::config::Config &core)
  {
    Config config;

    const auto limit = [&core](const char *key, std::size_t fallback)
    {
      const int value = core.getInt(key, static_cast<int>(fallback));
      if (value < 0)
      {
        throw Error{ErrorCode::InvalidConfiguration,
                    "realtime limits cannot be negative"};
      }
      return static_cast<std::size_t>(value);
    };
    const auto milliseconds = [&core](
                                 const char *key,
                                 std::chrono::milliseconds fallback)
    {
      return std::chrono::milliseconds{
          core.getInt(key, static_cast<int>(fallback.count()))};
    };
    const auto seconds = [&core](
                           const char *key,
                           std::chrono::seconds fallback)
    {
      return std::chrono::seconds{
          core.getInt(key, static_cast<int>(fallback.count()))};
    };

    config.maxActiveRooms = limit("realtime.max_active_rooms", config.maxActiveRooms);
    config.maxSessions = limit("realtime.max_sessions", config.maxSessions);
    config.maxSessionsPerRoom = limit("realtime.max_sessions_per_room", config.maxSessionsPerRoom);
    config.maxRoomsPerSession = limit("realtime.max_rooms_per_session", config.maxRoomsPerSession);
    config.maxPendingCommandsPerRoom = limit("realtime.max_pending_commands_per_room", config.maxPendingCommandsPerRoom);
    config.maxReplayEvents = limit("realtime.max_replay_events", config.maxReplayEvents);
    config.maxReplayBytes = limit("realtime.max_replay_bytes", config.maxReplayBytes);
    config.maxResumeRooms = limit("realtime.max_resume_rooms", config.maxResumeRooms);
    config.snapshotEveryEvents = limit("realtime.snapshot_every_events", config.snapshotEveryEvents);
    config.snapshotsToKeep = limit("realtime.snapshots_to_keep", config.snapshotsToKeep);
    config.roomIdleTimeout = milliseconds("realtime.room_idle_timeout_ms", config.roomIdleTimeout);
    config.sessionResumeWindow = seconds("realtime.session_resume_window_seconds", config.sessionResumeWindow);
    config.presenceTimeout = seconds("realtime.presence_timeout_seconds", config.presenceTimeout);
    config.replayTimeout = milliseconds("realtime.replay_timeout_ms", config.replayTimeout);
    config.snapshotOnRoomClose = core.getBool("realtime.snapshot_on_room_close", config.snapshotOnRoomClose);
    config.restoreRoomsOnOpen = core.getBool("realtime.restore_rooms_on_open", config.restoreRoomsOnOpen);
    config.enableSessionResume = core.getBool("realtime.enable_session_resume", config.enableSessionResume);
    config.enablePresence = core.getBool("realtime.enable_presence", config.enablePresence);

    config.validate();
    return config;
  }

  void Config::validate() const
  {
    if (maxActiveRooms == 0 ||
        maxSessions == 0 ||
        maxSessionsPerRoom == 0 ||
        maxRoomsPerSession == 0 ||
        maxPendingCommandsPerRoom == 0 ||
        maxReplayEvents == 0 ||
        maxReplayBytes == 0 ||
        maxResumeRooms == 0 ||
        snapshotsToKeep == 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "realtime limits must be greater than zero"};
    }

    if (sessionResumeWindow.count() < 0 ||
        presenceTimeout.count() < 0 ||
        replayTimeout.count() < 0 ||
        roomIdleTimeout.count() < 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "realtime durations cannot be negative"};
    }

  }
} // namespace vix::realtime
