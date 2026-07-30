/**
 *
 * @file local_presence_expiration_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for expiring records from the local presence store.
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
              "connection-42"},
          joinedAt,
          {}};
    }

    TEST(LocalPresenceExpirationTest, EmptyStorePrunesNothing)
    {
      LocalPresenceStore store;

      const std::size_t removed =
          store.prune_expired(
              Timestamp{
                  std::chrono::seconds{
                      2000}},
              std::chrono::seconds{
                  60});

      EXPECT_EQ(
          removed,
          0U);

      EXPECT_EQ(
          store.count(),
          0U);
    }

    TEST(LocalPresenceExpirationTest, KeepsRecentlyActivePresence)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1950}}));

      const std::size_t removed =
          store.prune_expired(
              Timestamp{
                  std::chrono::seconds{
                      2000}},
              std::chrono::seconds{
                  60});

      EXPECT_EQ(
          removed,
          0U);

      EXPECT_EQ(
          store.count(),
          1U);

      EXPECT_TRUE(
          store.find(
                   make_room_id(),
                   make_session_id())
              .has_value());
    }

    TEST(LocalPresenceExpirationTest, RemovesStalePresentPresence)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1000}}));

      ASSERT_EQ(
          store.count(),
          1U);

      const std::size_t removed =
          store.prune_expired(
              Timestamp{
                  std::chrono::seconds{
                      2000}},
              std::chrono::seconds{
                  60});

      EXPECT_EQ(
          removed,
          1U);

      EXPECT_EQ(
          store.count(),
          0U);

      EXPECT_FALSE(
          store.find(
                   make_room_id(),
                   make_session_id())
              .has_value());
    }

    TEST(LocalPresenceExpirationTest, UsesLastSeenInsteadOfJoinTimestamp)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence(
              make_room_id(),
              make_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1000}});

      presence.touch(
          Timestamp{
              std::chrono::seconds{
                  1980}});

      store.upsert(
          std::move(presence));

      const std::size_t removed =
          store.prune_expired(
              Timestamp{
                  std::chrono::seconds{
                      2000}},
              std::chrono::seconds{
                  60});

      EXPECT_EQ(
          removed,
          0U);

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
                  1980}});
    }

    TEST(LocalPresenceExpirationTest, RemovesStaleDetachedPresence)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence(
              make_room_id(),
              make_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1000}});

      presence.mark_detached(
          Timestamp{
              std::chrono::seconds{
                  1100}});

      store.upsert(
          std::move(presence));

      const std::size_t removed =
          store.prune_expired(
              Timestamp{
                  std::chrono::seconds{
                      2000}},
              std::chrono::seconds{
                  60});

      EXPECT_EQ(
          removed,
          1U);

      EXPECT_FALSE(
          store.find(
                   make_room_id(),
                   make_session_id())
              .has_value());
    }

    TEST(LocalPresenceExpirationTest, KeepsRecentlyDetachedPresence)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence(
              make_room_id(),
              make_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1000}});

      presence.mark_detached(
          Timestamp{
              std::chrono::seconds{
                  1980}});

      store.upsert(
          std::move(presence));

      const std::size_t removed =
          store.prune_expired(
              Timestamp{
                  std::chrono::seconds{
                      2000}},
              std::chrono::seconds{
                  60});

      EXPECT_EQ(
          removed,
          0U);

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_TRUE(
          stored->detached());

      EXPECT_TRUE(
          stored->logically_present());
    }

    TEST(LocalPresenceExpirationTest, RemovesOnlyExpiredRecords)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1000}}));

      store.upsert(
          make_presence(
              make_room_id(),
              make_other_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1980}}));

      const std::size_t removed =
          store.prune_expired(
              Timestamp{
                  std::chrono::seconds{
                      2000}},
              std::chrono::seconds{
                  60});

      EXPECT_EQ(
          removed,
          1U);

      EXPECT_EQ(
          store.count(),
          1U);

      EXPECT_FALSE(
          store.find(
                   make_room_id(),
                   make_session_id())
              .has_value());

      EXPECT_TRUE(
          store.find(
                   make_room_id(),
                   make_other_session_id())
              .has_value());
    }

    TEST(LocalPresenceExpirationTest, PrunesAcrossMultipleRooms)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1000}}));

      store.upsert(
          make_presence(
              make_other_room_id(),
              make_other_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1100}}));

      const std::size_t removed =
          store.prune_expired(
              Timestamp{
                  std::chrono::seconds{
                      2000}},
              std::chrono::seconds{
                  60});

      EXPECT_EQ(
          removed,
          2U);

      EXPECT_EQ(
          store.count(),
          0U);

      EXPECT_EQ(
          store.count_room(
              make_room_id()),
          0U);

      EXPECT_EQ(
          store.count_room(
              make_other_room_id()),
          0U);
    }

    TEST(LocalPresenceExpirationTest, UpdatesRoomListingsAfterPruning)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1000}}));

      store.upsert(
          make_presence(
              make_room_id(),
              make_other_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1980}}));

      ASSERT_EQ(
          store.list_room(
                   make_room_id())
              .size(),
          2U);

      store.prune_expired(
          Timestamp{
              std::chrono::seconds{
                  2000}},
          std::chrono::seconds{
              60});

      const auto visible =
          store.list_room(
              make_room_id());

      ASSERT_EQ(
          visible.size(),
          1U);

      EXPECT_EQ(
          visible.front()
              .session_id(),
          make_other_session_id());
    }

    TEST(LocalPresenceExpirationTest, UpdatesSessionListingsAfterPruning)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1000}}));

      store.upsert(
          make_presence(
              make_other_room_id(),
              make_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1980}}));

      ASSERT_EQ(
          store.list_session(
                   make_session_id())
              .size(),
          2U);

      store.prune_expired(
          Timestamp{
              std::chrono::seconds{
                  2000}},
          std::chrono::seconds{
              60});

      const auto remaining =
          store.list_session(
              make_session_id());

      ASSERT_EQ(
          remaining.size(),
          1U);

      EXPECT_EQ(
          remaining.front()
              .room_id(),
          make_other_room_id());
    }

    TEST(LocalPresenceExpirationTest, ZeroTimeoutRemovesOlderRecords)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              Timestamp{
                  std::chrono::seconds{
                      1000}}));

      const std::size_t removed =
          store.prune_expired(
              Timestamp{
                  std::chrono::seconds{
                      1001}},
              std::chrono::seconds{
                  0});

      EXPECT_EQ(
          removed,
          1U);

      EXPECT_EQ(
          store.count(),
          0U);
    }

  } // namespace

} // namespace vix::realtime
