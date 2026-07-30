/**
 *
 * @file event_audience_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime event audiences.
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

#include <cstdint>
#include <string_view>
#include <type_traits>

#include <vix/realtime/event_audience.hpp>

namespace vix::realtime
{
  namespace
  {
    TEST(EventAudienceTest, UsesUnsignedByteStorage)
    {
      EXPECT_TRUE(
          (std::is_same_v<
              std::underlying_type_t<EventAudience>,
              std::uint8_t>));
    }

    TEST(EventAudienceTest, ValuesAreStable)
    {
      EXPECT_EQ(
          static_cast<std::uint8_t>(
              EventAudience::Room),
          std::uint8_t{0});

      EXPECT_EQ(
          static_cast<std::uint8_t>(
              EventAudience::Sender),
          std::uint8_t{1});

      EXPECT_EQ(
          static_cast<std::uint8_t>(
              EventAudience::Others),
          std::uint8_t{2});

      EXPECT_EQ(
          static_cast<std::uint8_t>(
              EventAudience::Session),
          std::uint8_t{3});

      EXPECT_EQ(
          static_cast<std::uint8_t>(
              EventAudience::Internal),
          std::uint8_t{4});
    }

    TEST(EventAudienceTest, ConvertsRoomAudienceToString)
    {
      EXPECT_EQ(
          to_string(
              EventAudience::Room),
          std::string_view{"room"});
    }

    TEST(EventAudienceTest, ConvertsSenderAudienceToString)
    {
      EXPECT_EQ(
          to_string(
              EventAudience::Sender),
          std::string_view{"sender"});
    }

    TEST(EventAudienceTest, ConvertsOthersAudienceToString)
    {
      EXPECT_EQ(
          to_string(
              EventAudience::Others),
          std::string_view{"others"});
    }

    TEST(EventAudienceTest, ConvertsSessionAudienceToString)
    {
      EXPECT_EQ(
          to_string(
              EventAudience::Session),
          std::string_view{"session"});
    }

    TEST(EventAudienceTest, ConvertsInternalAudienceToString)
    {
      EXPECT_EQ(
          to_string(
              EventAudience::Internal),
          std::string_view{"internal"});
    }

    TEST(EventAudienceTest, UnknownValueFallsBackToRoom)
    {
      const auto unknown =
          static_cast<EventAudience>(
              std::uint8_t{255});

      EXPECT_EQ(
          to_string(unknown),
          std::string_view{"room"});
    }

    TEST(EventAudienceTest, StringConversionIsConstexpr)
    {
      constexpr std::string_view room =
          to_string(
              EventAudience::Room);

      constexpr std::string_view sender =
          to_string(
              EventAudience::Sender);

      constexpr std::string_view others =
          to_string(
              EventAudience::Others);

      constexpr std::string_view session =
          to_string(
              EventAudience::Session);

      constexpr std::string_view internal =
          to_string(
              EventAudience::Internal);

      static_assert(
          room == "room");

      static_assert(
          sender == "sender");

      static_assert(
          others == "others");

      static_assert(
          session == "session");

      static_assert(
          internal == "internal");

      SUCCEED();
    }

  } // namespace

} // namespace vix::realtime
