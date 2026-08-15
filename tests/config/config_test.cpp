#include <gtest/gtest.h>

#include <chrono>

#include <vix/config/Config.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  TEST(RealtimeConfigTest, FromCoreMapsEverySupportedField)
  {
    vix::config::Config core;
    core.set("realtime.max_active_rooms", 12);
    core.set("realtime.max_sessions", 13);
    core.set("realtime.max_sessions_per_room", 14);
    core.set("realtime.max_rooms_per_session", 15);
    core.set("realtime.max_pending_commands_per_room", 16);
    core.set("realtime.max_replay_events", 17);
    core.set("realtime.max_replay_bytes", 18);
    core.set("realtime.max_resume_rooms", 19);
    core.set("realtime.snapshot_every_events", 0);
    core.set("realtime.snapshots_to_keep", 2);
    core.set("realtime.room_idle_timeout_ms", 20);
    core.set("realtime.session_resume_window_seconds", 21);
    core.set("realtime.presence_timeout_seconds", 22);
    core.set("realtime.replay_timeout_ms", 23);
    core.set("realtime.snapshot_on_room_close", false);
    core.set("realtime.restore_rooms_on_open", false);
    core.set("realtime.enable_session_resume", false);
    core.set("realtime.enable_presence", false);

    const Config config = Config::from_core(core);
    EXPECT_EQ(config.maxActiveRooms, 12U);
    EXPECT_EQ(config.maxSessions, 13U);
    EXPECT_EQ(config.maxSessionsPerRoom, 14U);
    EXPECT_EQ(config.maxRoomsPerSession, 15U);
    EXPECT_EQ(config.maxPendingCommandsPerRoom, 16U);
    EXPECT_EQ(config.maxReplayEvents, 17U);
    EXPECT_EQ(config.maxReplayBytes, 18U);
    EXPECT_EQ(config.maxResumeRooms, 19U);
    EXPECT_EQ(config.snapshotEveryEvents, 0U);
    EXPECT_EQ(config.snapshotsToKeep, 2U);
    EXPECT_EQ(config.roomIdleTimeout, std::chrono::milliseconds{20});
    EXPECT_EQ(config.sessionResumeWindow, std::chrono::seconds{21});
    EXPECT_EQ(config.presenceTimeout, std::chrono::seconds{22});
    EXPECT_EQ(config.replayTimeout, std::chrono::milliseconds{23});
    EXPECT_FALSE(config.snapshotOnRoomClose);
    EXPECT_FALSE(config.restoreRoomsOnOpen);
    EXPECT_FALSE(config.enableSessionResume);
    EXPECT_FALSE(config.enablePresence);
  }

  TEST(RealtimeConfigTest, AllowsDisabledPeriodicSnapshots)
  {
    Config config;
    config.snapshotEveryEvents = 0;
    EXPECT_NO_THROW(config.validate());
  }

  TEST(RealtimeConfigTest, RejectsInvalidReplayLimits)
  {
    Config config;
    config.replayTimeout = std::chrono::milliseconds{-1};
    EXPECT_THROW(config.validate(), Error);
  }
} // namespace vix::realtime
