/**
 *
 * @file local_presence_visibility_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for visibility rules in the local presence store.
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

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

    [[nodiscard]] SessionId make_second_session_id()
    {
      return SessionId{
          std::string_view{
              "session-84"}};
    }

    [[nodiscard]] SessionId make_third_session_id()
    {
      return SessionId{
          std::string_view{
              "session-126"}};
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
        std::string identity,
        std::string connectionId,
        std::int64_t joinedAt)
    {
      return Presence{
          std::move(roomId),
          std::move(sessionId),
          Identity{
              std::move(identity)},
          std::optional<NodeId>{
              make_node_id()},
          ConnectionId{
              std::move(connectionId)},
          Timestamp{
              std::chrono::seconds{
                  joinedAt}},
          {}};
    }

    [[nodiscard]] bool contains_session(
        const std::vector<Presence> &presences,
        const SessionId &sessionId)
    {
      return std::any_of(
          presences.begin(),
          presences.end(),
          [&sessionId](
              const Presence &presence)
          {
            return presence.session_id() ==
                   sessionId;
          });
    }

    TEST(LocalPresenceVisibilityTest, PresentRecordIsVisibleInRoom)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      const auto visible =
          store.list_room(
              make_room_id());

      ASSERT_EQ(
          visible.size(),
          1U);

      EXPECT_EQ(
          visible.front()
              .session_id(),
          make_session_id());

      EXPECT_TRUE(
          visible.front()
              .logically_present());
    }

    TEST(LocalPresenceVisibilityTest, MultiplePresentRecordsAreVisible)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      store.upsert(
          make_presence(
              make_room_id(),
              make_second_session_id(),
              "citizen-84",
              "connection-84",
              1001));

      const auto visible =
          store.list_room(
              make_room_id());

      ASSERT_EQ(
          visible.size(),
          2U);

      EXPECT_TRUE(
          contains_session(
              visible,
              make_session_id()));

      EXPECT_TRUE(
          contains_session(
              visible,
              make_second_session_id()));
    }

    TEST(LocalPresenceVisibilityTest, DetachedRecordRemainsLogicallyVisible)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000);

      presence.mark_detached(
          Timestamp{
              std::chrono::seconds{
                  1100}});

      store.upsert(
          std::move(presence));

      const auto visible =
          store.list_room(
              make_room_id());

      ASSERT_EQ(
          visible.size(),
          1U);

      EXPECT_EQ(
          visible.front()
              .status(),
          PresenceStatus::Detached);

      EXPECT_TRUE(
          visible.front()
              .logically_present());

      EXPECT_FALSE(
          visible.front()
              .connected());
    }

    TEST(LocalPresenceVisibilityTest, LeftRecordIsNotVisibleInRoom)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000);

      presence.mark_left(
          Timestamp{
              std::chrono::seconds{
                  1100}});

      store.upsert(
          std::move(presence));

      EXPECT_TRUE(
          store.list_room(
                   make_room_id())
              .empty());

      EXPECT_EQ(
          store.count_room(
              make_room_id()),
          0U);
    }

    TEST(LocalPresenceVisibilityTest, LeftRecordDoesNotHideOtherSessions)
    {
      LocalPresenceStore store;

      Presence left =
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000);

      left.mark_left(
          Timestamp{
              std::chrono::seconds{
                  1100}});

      store.upsert(
          std::move(left));

      store.upsert(
          make_presence(
              make_room_id(),
              make_second_session_id(),
              "citizen-84",
              "connection-84",
              1001));

      const auto visible =
          store.list_room(
              make_room_id());

      ASSERT_EQ(
          visible.size(),
          1U);

      EXPECT_EQ(
          visible.front()
              .session_id(),
          make_second_session_id());
    }

    TEST(LocalPresenceVisibilityTest, RoomVisibilityIsIsolated)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      store.upsert(
          make_presence(
              make_other_room_id(),
              make_second_session_id(),
              "citizen-84",
              "connection-84",
              1001));

      const auto river =
          store.list_room(
              make_room_id());

      const auto library =
          store.list_room(
              make_other_room_id());

      ASSERT_EQ(
          river.size(),
          1U);

      ASSERT_EQ(
          library.size(),
          1U);

      EXPECT_EQ(
          river.front()
              .session_id(),
          make_session_id());

      EXPECT_EQ(
          library.front()
              .session_id(),
          make_second_session_id());
    }

    TEST(LocalPresenceVisibilityTest, SessionVisibilityIncludesMultipleRooms)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      store.upsert(
          make_presence(
              make_other_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1001));

      const auto visible =
          store.list_session(
              make_session_id());

      ASSERT_EQ(
          visible.size(),
          2U);

      EXPECT_TRUE(
          std::any_of(
              visible.begin(),
              visible.end(),
              [](const Presence &presence)
              {
                return presence.room_id() ==
                       make_room_id();
              }));

      EXPECT_TRUE(
          std::any_of(
              visible.begin(),
              visible.end(),
              [](const Presence &presence)
              {
                return presence.room_id() ==
                       make_other_room_id();
              }));
    }

    TEST(LocalPresenceVisibilityTest, SessionVisibilityExcludesLeftRooms)
    {
      LocalPresenceStore store;

      Presence left =
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000);

      left.mark_left(
          Timestamp{
              std::chrono::seconds{
                  1100}});

      store.upsert(
          std::move(left));

      store.upsert(
          make_presence(
              make_other_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1001));

      const auto visible =
          store.list_session(
              make_session_id());

      ASSERT_EQ(
          visible.size(),
          1U);

      EXPECT_EQ(
          visible.front()
              .room_id(),
          make_other_room_id());
    }

    TEST(LocalPresenceVisibilityTest, DetachedAndPresentRecordsCanCoexist)
    {
      LocalPresenceStore store;

      Presence detached =
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000);

      detached.mark_detached(
          Timestamp{
              std::chrono::seconds{
                  1100}});

      store.upsert(
          std::move(detached));

      store.upsert(
          make_presence(
              make_room_id(),
              make_second_session_id(),
              "citizen-84",
              "connection-84",
              1001));

      const auto visible =
          store.list_room(
              make_room_id());

      ASSERT_EQ(
          visible.size(),
          2U);

      EXPECT_TRUE(
          contains_session(
              visible,
              make_session_id()));

      EXPECT_TRUE(
          contains_session(
              visible,
              make_second_session_id()));
    }

    TEST(LocalPresenceVisibilityTest, LeavingRemovesOnlyTargetFromVisibility)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      store.upsert(
          make_presence(
              make_room_id(),
              make_second_session_id(),
              "citizen-84",
              "connection-84",
              1001));

      store.upsert(
          make_presence(
              make_room_id(),
              make_third_session_id(),
              "citizen-126",
              "connection-126",
              1002));

      auto leaving =
          store.find(
              make_room_id(),
              make_second_session_id());

      ASSERT_TRUE(
          leaving.has_value());

      leaving->mark_left(
          Timestamp{
              std::chrono::seconds{
                  1200}});

      store.upsert(
          std::move(*leaving));

      const auto visible =
          store.list_room(
              make_room_id());

      ASSERT_EQ(
          visible.size(),
          2U);

      EXPECT_TRUE(
          contains_session(
              visible,
              make_session_id()));

      EXPECT_FALSE(
          contains_session(
              visible,
              make_second_session_id()));

      EXPECT_TRUE(
          contains_session(
              visible,
              make_third_session_id()));
    }

    TEST(LocalPresenceVisibilityTest, RejoinRestoresVisibility)
    {
      LocalPresenceStore store;

      Presence presence =
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000);

      presence.mark_left(
          Timestamp{
              std::chrono::seconds{
                  1100}});

      store.upsert(
          std::move(presence));

      ASSERT_TRUE(
          store.list_room(
                   make_room_id())
              .empty());

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-84",
              1200));

      const auto visible =
          store.list_room(
              make_room_id());

      ASSERT_EQ(
          visible.size(),
          1U);

      EXPECT_EQ(
          visible.front()
              .status(),
          PresenceStatus::Present);

      EXPECT_EQ(
          visible.front()
              .connection_id(),
          "connection-84");
    }

  } // namespace

} // namespace vix::realtime
