/**
 *
 * @file local_presence_heartbeat_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for heartbeats in the Vix Realtime local presence store.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/local_presence_store.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/presence.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    [[nodiscard]] RoomId make_room_id()
    {
      return RoomId{
          std::string_view{
              "city/river"}};
    }

    [[nodiscard]] RoomId make_other_room_id()
    {
      return RoomId{
          std::string_view{
              "city/library"}};
    }

    [[nodiscard]] SessionId make_session_id()
    {
      return SessionId{
          std::string_view{
              "session-42"}};
    }

    [[nodiscard]] SessionId make_other_session_id()
    {
      return SessionId{
          std::string_view{
              "session-84"}};
    }

    [[nodiscard]] NodeId make_node_id()
    {
      return NodeId{
          std::string_view{
              "node-1"}};
    }

    [[nodiscard]] Presence make_presence(
        RoomId roomId,
        SessionId sessionId,
        std::string connectionId,
        Timestamp joinedAt)
    {
      return Presence{
          std::move(roomId),
          std::move(sessionId),
          Identity{
              "citizen-42"},
          std::optional<NodeId>{
              make_node_id()},
          ConnectionId{
              std::move(connectionId)},
          joinedAt,
          {}};
    }

    [[nodiscard]] Presence make_presence()
    {
      return make_presence(
          make_room_id(),
          make_session_id(),
          "connection-42",
          Timestamp{
              std::chrono::seconds{
                  1000}});
    }

    TEST(LocalPresenceHeartbeatTest, UpdatesStoredLastSeenTimestamp)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence());

      auto presence =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          presence.has_value());

      const Timestamp heartbeatAt{
          std::chrono::seconds{
              1100}};

      presence->touch(
          heartbeatAt);

      store.upsert(
          std::move(*presence));

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_EQ(
          stored->last_seen_at(),
          heartbeatAt);
    }

    TEST(LocalPresenceHeartbeatTest, HeartbeatKeepsPresencePresent)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence());

      auto presence =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          presence.has_value());

      presence->touch(
          Timestamp{
              std::chrono::seconds{
                  1100}});

      store.upsert(
          std::move(*presence));

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_EQ(
          stored->status(),
          PresenceStatus::Present);

      EXPECT_TRUE(
          stored->logically_present());

      EXPECT_TRUE(
          stored->connected());

      EXPECT_FALSE(
          stored->detached());

      EXPECT_FALSE(
          stored->left());
    }

    TEST(LocalPresenceHeartbeatTest, HeartbeatPreservesJoinTimestamp)
    {
      LocalPresenceStore store;

      const Timestamp joinedAt{
          std::chrono::seconds{
              1000}};

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "connection-42",
              joinedAt));

      auto presence =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          presence.has_value());

      presence->touch(
          Timestamp{
              std::chrono::seconds{
                  1200}});

      store.upsert(
          std::move(*presence));

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_EQ(
          stored->joined_at(),
          joinedAt);

      EXPECT_EQ(
          stored->last_seen_at(),
          Timestamp{
              std::chrono::seconds{
                  1200}});
    }

    TEST(LocalPresenceHeartbeatTest, RepeatedHeartbeatsKeepLatestTimestamp)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence());

      for (const auto value :
           {1100, 1200, 1300})
      {
        auto presence =
            store.find(
                make_room_id(),
                make_session_id());

        ASSERT_TRUE(
            presence.has_value());

        presence->touch(
            Timestamp{
                std::chrono::seconds{
                    value}});

        store.upsert(
            std::move(*presence));
      }

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_EQ(
          stored->last_seen_at(),
          Timestamp{
              std::chrono::seconds{
                  1300}});
    }

    TEST(LocalPresenceHeartbeatTest, HeartbeatDoesNotCreateDuplicateRecord)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence());

      auto presence =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          presence.has_value());

      presence->touch(
          Timestamp{
              std::chrono::seconds{
                  1100}});

      store.upsert(
          std::move(*presence));

      EXPECT_EQ(
          store.count(),
          1U);

      EXPECT_EQ(
          store.count_room(
              make_room_id()),
          1U);
    }

    TEST(LocalPresenceHeartbeatTest, HeartbeatKeepsPresenceActive)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence());

      auto presence =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          presence.has_value());

      const Timestamp heartbeatAt{
          std::chrono::seconds{
              1100}};

      presence->touch(
          heartbeatAt);

      store.upsert(
          std::move(*presence));

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_TRUE(
          stored->active(
              Timestamp{
                  std::chrono::seconds{
                      1150}},
              std::chrono::seconds{
                  60}));

      EXPECT_FALSE(
          stored->stale(
              Timestamp{
                  std::chrono::seconds{
                      1150}},
              std::chrono::seconds{
                  60}));
    }

    TEST(LocalPresenceHeartbeatTest, PresenceBecomesStaleWithoutHeartbeat)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence());

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_TRUE(
          stored->stale(
              Timestamp{
                  std::chrono::seconds{
                      1100}},
              std::chrono::seconds{
                  60}));

      EXPECT_FALSE(
          stored->active(
              Timestamp{
                  std::chrono::seconds{
                      1100}},
              std::chrono::seconds{
                  60}));
    }

    TEST(LocalPresenceHeartbeatTest, HeartbeatAffectsOnlyTargetPresence)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "connection-42",
              Timestamp{
                  std::chrono::seconds{
                      1000}}));

      store.upsert(
          make_presence(
              make_room_id(),
              make_other_session_id(),
              "connection-84",
              Timestamp{
                  std::chrono::seconds{
                      1000}}));

      auto first =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          first.has_value());

      first->touch(
          Timestamp{
              std::chrono::seconds{
                  1200}});

      store.upsert(
          std::move(*first));

      const auto storedFirst =
          store.find(
              make_room_id(),
              make_session_id());

      const auto storedSecond =
          store.find(
              make_room_id(),
              make_other_session_id());

      ASSERT_TRUE(
          storedFirst.has_value());

      ASSERT_TRUE(
          storedSecond.has_value());

      EXPECT_EQ(
          storedFirst->last_seen_at(),
          Timestamp{
              std::chrono::seconds{
                  1200}});

      EXPECT_EQ(
          storedSecond->last_seen_at(),
          Timestamp{
              std::chrono::seconds{
                  1000}});
    }

    TEST(LocalPresenceHeartbeatTest, HeartbeatAffectsOnlyTargetRoom)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "connection-42",
              Timestamp{
                  std::chrono::seconds{
                      1000}}));

      store.upsert(
          make_presence(
              make_other_room_id(),
              make_session_id(),
              "connection-42",
              Timestamp{
                  std::chrono::seconds{
                      1000}}));

      auto river =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          river.has_value());

      river->touch(
          Timestamp{
              std::chrono::seconds{
                  1200}});

      store.upsert(
          std::move(*river));

      const auto storedRiver =
          store.find(
              make_room_id(),
              make_session_id());

      const auto storedLibrary =
          store.find(
              make_other_room_id(),
              make_session_id());

      ASSERT_TRUE(
          storedRiver.has_value());

      ASSERT_TRUE(
          storedLibrary.has_value());

      EXPECT_EQ(
          storedRiver->last_seen_at(),
          Timestamp{
              std::chrono::seconds{
                  1200}});

      EXPECT_EQ(
          storedLibrary->last_seen_at(),
          Timestamp{
              std::chrono::seconds{
                  1000}});
    }

    TEST(LocalPresenceHeartbeatTest, DetachedPresenceRetainsDetachedStatus)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence();

      presence.mark_detached(
          Timestamp{
              std::chrono::seconds{
                  1100}});

      store.upsert(
          std::move(presence));

      auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      stored->touch(
          Timestamp{
              std::chrono::seconds{
                  1200}});

      store.upsert(
          std::move(*stored));

      const auto updated =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          updated.has_value());

      EXPECT_EQ(
          updated->status(),
          PresenceStatus::Detached);

      EXPECT_TRUE(
          updated->detached());

      EXPECT_FALSE(
          updated->connected());

      EXPECT_EQ(
          updated->last_seen_at(),
          Timestamp{
              std::chrono::seconds{
                  1200}});
    }

    TEST(LocalPresenceHeartbeatTest, LeftPresenceRejectsHeartbeat)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence();

      presence.mark_left(
          Timestamp{
              std::chrono::seconds{
                  1100}});

      store.upsert(
          std::move(presence));

      auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_THROW(
          stored->touch(
              Timestamp{
                  std::chrono::seconds{
                      1200}}),
          Error);
    }

  } // namespace

} // namespace vix::realtime
