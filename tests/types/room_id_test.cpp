/**
 *
 * @file room_id_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime room identifier.
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

#include <functional>
#include <string>
#include <unordered_set>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/room_id.hpp>

namespace vix::realtime
{
  namespace
  {
    TEST(RoomIdTest, DefaultIdentifierIsEmpty)
    {
      const RoomId roomId;

      EXPECT_TRUE(roomId.empty());
      EXPECT_TRUE(roomId.value().empty());
    }

    TEST(RoomIdTest, AcceptsSimpleIdentifier)
    {
      const RoomId roomId{"general"};

      EXPECT_FALSE(roomId.empty());
      EXPECT_EQ(roomId.value(), "general");
    }

    TEST(RoomIdTest, AcceptsSupportedCharacters)
    {
      const RoomId roomId{
          "workspace_1/chat-room.main:public"};

      EXPECT_EQ(
          roomId.value(),
          "workspace_1/chat-room.main:public");
    }

    TEST(RoomIdTest, AcceptsNestedRoomPath)
    {
      const RoomId roomId{
          "human-city/district-1/river"};

      EXPECT_EQ(
          roomId.value(),
          "human-city/district-1/river");
    }

    TEST(RoomIdTest, AcceptsMaximumLength)
    {
      const std::string value(
          128,
          'a');

      const RoomId roomId{value};

      EXPECT_EQ(
          roomId.value().size(),
          128U);

      EXPECT_EQ(
          roomId.value(),
          value);
    }

    TEST(RoomIdTest, RejectsIdentifierAboveMaximumLength)
    {
      const std::string value(
          129,
          'a');

      EXPECT_THROW(
          static_cast<void>(
              RoomId{value}),
          Error);
    }

    TEST(RoomIdTest, RejectsWhitespace)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomId{"general room"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"general\troom"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"general\nroom"}),
          Error);
    }

    TEST(RoomIdTest, RejectsUnsupportedCharacters)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomId{"room@node"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"room#main"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"room?main"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"room\\main"}),
          Error);
    }

    TEST(RoomIdTest, RejectsLeadingSeparator)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomId{"/general"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"-general"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"_general"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{".general"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{":general"}),
          Error);
    }

    TEST(RoomIdTest, RejectsTrailingSeparator)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomId{"general/"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"general-"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"general_"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"general."}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"general:"}),
          Error);
    }

    TEST(RoomIdTest, RejectsConsecutiveSeparators)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomId{"human//city"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"human--city"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"human__city"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"human..city"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"human::city"}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomId{"human/-city"}),
          Error);
    }

    TEST(RoomIdTest, SupportsEquality)
    {
      const RoomId first{"chat/general"};
      const RoomId second{"chat/general"};
      const RoomId different{"chat/private"};

      EXPECT_EQ(first, second);
      EXPECT_NE(first, different);
    }

    TEST(RoomIdTest, SupportsLexicographicalOrdering)
    {
      const RoomId first{"chat/general"};
      const RoomId second{"chat/private"};

      EXPECT_LT(first, second);
      EXPECT_GT(second, first);
      EXPECT_LE(first, second);
      EXPECT_GE(second, first);
    }

    TEST(RoomIdTest, HashIsStableForEqualIdentifiers)
    {
      const RoomId first{"chat/general"};
      const RoomId second{"chat/general"};

      const std::hash<RoomId> hash;

      EXPECT_EQ(
          hash(first),
          hash(second));
    }

    TEST(RoomIdTest, CanBeUsedInUnorderedContainers)
    {
      std::unordered_set<RoomId> roomIds;

      roomIds.emplace("chat/general");
      roomIds.emplace("chat/private");
      roomIds.emplace("chat/general");

      EXPECT_EQ(
          roomIds.size(),
          2U);

      EXPECT_EQ(
          roomIds.count(
              RoomId{"chat/general"}),
          1U);

      EXPECT_EQ(
          roomIds.count(
              RoomId{"unknown"}),
          0U);
    }

  } // namespace

} // namespace vix::realtime
