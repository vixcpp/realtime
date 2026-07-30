/**
 *
 * @file session_rooms_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for room memberships owned by Vix Realtime sessions.
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
#include <string_view>

#include <vix/realtime/room_id.hpp>
#include <vix/realtime/session.hpp>
#include <vix/realtime/session_id.hpp>

namespace vix::realtime
{
  namespace
  {
    [[nodiscard]] Session make_session()
    {
      return Session{
          SessionId{
              std::string_view{
                  "session-42"}}};
    }

    [[nodiscard]] RoomId make_general_room()
    {
      return RoomId{
          std::string_view{
              "chat/general"}};
    }

    [[nodiscard]] RoomId make_private_room()
    {
      return RoomId{
          std::string_view{
              "chat/private"}};
    }

    [[nodiscard]] RoomId make_lobby_room()
    {
      return RoomId{
          std::string_view{
              "game/lobby"}};
    }

    TEST(SessionRoomsTest, StartsWithoutRoomMemberships)
    {
      const Session session =
          make_session();

      EXPECT_EQ(
          session.room_count(),
          0U);

      EXPECT_TRUE(
          session.rooms().empty());

      EXPECT_FALSE(
          session.has_room(
              make_general_room()));
    }

    TEST(SessionRoomsTest, JoinsRoom)
    {
      Session session =
          make_session();

      EXPECT_TRUE(
          session.join_room(
              make_general_room()));

      EXPECT_TRUE(
          session.has_room(
              make_general_room()));

      EXPECT_EQ(
          session.room_count(),
          1U);
    }

    TEST(SessionRoomsTest, JoiningSameRoomTwiceReturnsFalse)
    {
      Session session =
          make_session();

      EXPECT_TRUE(
          session.join_room(
              make_general_room()));

      EXPECT_FALSE(
          session.join_room(
              make_general_room()));

      EXPECT_EQ(
          session.room_count(),
          1U);
    }

    TEST(SessionRoomsTest, JoinsMultipleRooms)
    {
      Session session =
          make_session();

      EXPECT_TRUE(
          session.join_room(
              make_general_room()));

      EXPECT_TRUE(
          session.join_room(
              make_private_room()));

      EXPECT_TRUE(
          session.join_room(
              make_lobby_room()));

      EXPECT_EQ(
          session.room_count(),
          3U);

      EXPECT_TRUE(
          session.has_room(
              make_general_room()));

      EXPECT_TRUE(
          session.has_room(
              make_private_room()));

      EXPECT_TRUE(
          session.has_room(
              make_lobby_room()));
    }

    TEST(SessionRoomsTest, ExposesJoinedRooms)
    {
      Session session =
          make_session();

      session.join_room(
          make_general_room());

      session.join_room(
          make_private_room());

      const auto &rooms =
          session.rooms();

      EXPECT_EQ(
          rooms.size(),
          2U);

      EXPECT_NE(
          rooms.find(
              make_general_room()),
          rooms.end());

      EXPECT_NE(
          rooms.find(
              make_private_room()),
          rooms.end());
    }

    TEST(SessionRoomsTest, LeavesJoinedRoom)
    {
      Session session =
          make_session();

      session.join_room(
          make_general_room());

      session.join_room(
          make_private_room());

      EXPECT_TRUE(
          session.leave_room(
              make_general_room()));

      EXPECT_FALSE(
          session.has_room(
              make_general_room()));

      EXPECT_TRUE(
          session.has_room(
              make_private_room()));

      EXPECT_EQ(
          session.room_count(),
          1U);
    }

    TEST(SessionRoomsTest, LeavingUnknownRoomReturnsFalse)
    {
      Session session =
          make_session();

      EXPECT_FALSE(
          session.leave_room(
              make_general_room()));

      EXPECT_EQ(
          session.room_count(),
          0U);
    }

    TEST(SessionRoomsTest, LeavingRoomTwiceReturnsFalse)
    {
      Session session =
          make_session();

      session.join_room(
          make_general_room());

      EXPECT_TRUE(
          session.leave_room(
              make_general_room()));

      EXPECT_FALSE(
          session.leave_room(
              make_general_room()));

      EXPECT_EQ(
          session.room_count(),
          0U);
    }

    TEST(SessionRoomsTest, ClearsAllRooms)
    {
      Session session =
          make_session();

      session.join_room(
          make_general_room());

      session.join_room(
          make_private_room());

      session.join_room(
          make_lobby_room());

      EXPECT_EQ(
          session.clear_rooms(),
          3U);

      EXPECT_EQ(
          session.room_count(),
          0U);

      EXPECT_TRUE(
          session.rooms().empty());
    }

    TEST(SessionRoomsTest, ClearingEmptyMembershipsReturnsZero)
    {
      Session session =
          make_session();

      EXPECT_EQ(
          session.clear_rooms(),
          0U);
    }

    TEST(SessionRoomsTest, MembershipsRemainIndependentBetweenSessions)
    {
      Session first{
          SessionId{
              std::string_view{
                  "session-1"}}};

      Session second{
          SessionId{
              std::string_view{
                  "session-2"}}};

      first.join_room(
          make_general_room());

      second.join_room(
          make_private_room());

      EXPECT_TRUE(
          first.has_room(
              make_general_room()));

      EXPECT_FALSE(
          first.has_room(
              make_private_room()));

      EXPECT_TRUE(
          second.has_room(
              make_private_room()));

      EXPECT_FALSE(
          second.has_room(
              make_general_room()));
    }

  } // namespace

} // namespace vix::realtime
