/**
 *
 * @file local_presence_leave_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for leaving rooms through the local presence store.
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

    TEST(LocalPresenceLeaveTest, StoresLeftPresenceRecord)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence();

      const Timestamp leftAt{
          std::chrono::seconds{
              1200}};

      presence.mark_left(
          leftAt);

      store.upsert(
          std::move(presence));

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_EQ(
          stored->status(),
          PresenceStatus::Left);

      EXPECT_TRUE(
          stored->left());

      EXPECT_FALSE(
          stored->logically_present());

      ASSERT_TRUE(
          stored->left_at()
              .has_value());

      EXPECT_EQ(
          *stored->left_at(),
          leftAt);
    }

    TEST(LocalPresenceLeaveTest, ReplacesPresentRecordWithLeftState)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence());

      ASSERT_EQ(
          store.count(),
          1U);

      auto presence =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          presence.has_value());

      presence->mark_left(
          Timestamp{
              std::chrono::seconds{
                  1200}});

      store.upsert(
          std::move(*presence));

      EXPECT_EQ(
          store.count(),
          1U);

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_TRUE(
          stored->left());

      EXPECT_FALSE(
          stored->connected());

      EXPECT_FALSE(
          stored->detached());
    }

    TEST(LocalPresenceLeaveTest, LeaveClearsConnectionIdentifier)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence();

      ASSERT_EQ(
          presence.connection_id(),
          "connection-42");

      presence.mark_left(
          Timestamp{
              std::chrono::seconds{
                  1200}});

      store.upsert(
          std::move(presence));

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_TRUE(
          stored->connection_id()
              .empty());

      EXPECT_FALSE(
          stored->connected());
    }

    TEST(LocalPresenceLeaveTest, LeaveUpdatesLastSeenTimestamp)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence();

      const Timestamp leftAt{
          std::chrono::seconds{
              1300}};

      presence.mark_left(
          leftAt);

      store.upsert(
          std::move(presence));

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_EQ(
          stored->last_seen_at(),
          leftAt);

      ASSERT_TRUE(
          stored->left_at()
              .has_value());

      EXPECT_EQ(
          *stored->left_at(),
          leftAt);
    }

    TEST(LocalPresenceLeaveTest, LeavingOneRoomDoesNotAffectOtherRoom)
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
                      1001}}));

      auto river =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          river.has_value());

      river->mark_left(
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

      EXPECT_TRUE(
          storedRiver->left());

      EXPECT_TRUE(
          storedLibrary->logically_present());

      EXPECT_TRUE(
          storedLibrary->connected());
    }

    TEST(LocalPresenceLeaveTest, LeavingOneSessionDoesNotAffectOtherSession)
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
                      1001}}));

      auto first =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          first.has_value());

      first->mark_left(
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

      EXPECT_TRUE(
          storedFirst->left());

      EXPECT_TRUE(
          storedSecond->logically_present());

      EXPECT_TRUE(
          storedSecond->connected());
    }

    TEST(LocalPresenceLeaveTest, LeftRecordCannotReceiveHeartbeat)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence();

      presence.mark_left(
          Timestamp{
              std::chrono::seconds{
                  1200}});

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
                      1300}}),
          Error);
    }

    TEST(LocalPresenceLeaveTest, LeftRecordCanBeReplacedByNewJoin)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence();

      presence.mark_left(
          Timestamp{
              std::chrono::seconds{
                  1200}});

      store.upsert(
          std::move(presence));

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "connection-84",
              Timestamp{
                  std::chrono::seconds{
                      1400}}));

      EXPECT_EQ(
          store.count(),
          1U);

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
          stored->connected());

      EXPECT_FALSE(
          stored->left());

      EXPECT_EQ(
          stored->connection_id(),
          "connection-84");

      EXPECT_EQ(
          stored->joined_at(),
          Timestamp{
              std::chrono::seconds{
                  1400}});

      EXPECT_FALSE(
          stored->left_at()
              .has_value());
    }

  } // namespace

} // namespace vix::realtime
