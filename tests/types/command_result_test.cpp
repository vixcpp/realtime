/**
 *
 * @file command_result_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime command results.
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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/realtime/command_result.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    [[nodiscard]] RoomId make_room_id()
    {
      return RoomId{
          std::string_view{
              "chat/general"}};
    }

    [[nodiscard]] RoomEvent make_event(
        std::string type = "message.sent")
    {
      JsonObject payload;

      payload.set_string(
          "message",
          "Hello");

      return RoomEvent{
          make_room_id(),
          std::move(type),
          std::move(payload),
          EventAudience::Room};
    }

    TEST(CommandStatusTest, ValuesAreStable)
    {
      EXPECT_EQ(
          static_cast<std::uint8_t>(
              CommandStatus::Accepted),
          std::uint8_t{0});

      EXPECT_EQ(
          static_cast<std::uint8_t>(
              CommandStatus::Rejected),
          std::uint8_t{1});

      EXPECT_EQ(
          static_cast<std::uint8_t>(
              CommandStatus::Ignored),
          std::uint8_t{2});
    }

    TEST(CommandStatusTest, ConvertsStatusesToString)
    {
      EXPECT_EQ(
          to_string(
              CommandStatus::Accepted),
          std::string_view{
              "accepted"});

      EXPECT_EQ(
          to_string(
              CommandStatus::Rejected),
          std::string_view{
              "rejected"});

      EXPECT_EQ(
          to_string(
              CommandStatus::Ignored),
          std::string_view{
              "ignored"});
    }

    TEST(CommandStatusTest, UnknownStatusFallsBackToRejected)
    {
      const auto unknown =
          static_cast<CommandStatus>(
              std::uint8_t{255});

      EXPECT_EQ(
          to_string(unknown),
          std::string_view{
              "rejected"});
    }

    TEST(CommandResultTest, CreatesAcceptedResultWithoutEvents)
    {
      const CommandResult result =
          CommandResult::accepted();

      EXPECT_EQ(
          result.status(),
          CommandStatus::Accepted);

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_FALSE(
          result.is_rejected());

      EXPECT_FALSE(
          result.is_ignored());

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::None);

      EXPECT_TRUE(
          result.message().empty());

      EXPECT_TRUE(
          result.events().empty());

      EXPECT_EQ(
          result.event_count(),
          0U);

      EXPECT_NO_THROW(
          result.validate());
    }

    TEST(CommandResultTest, CreatesAcceptedResultWithOneEvent)
    {
      const CommandResult result =
          CommandResult::accepted(
              {make_event()});

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          result.event_count(),
          1U);

      ASSERT_EQ(
          result.events().size(),
          1U);

      EXPECT_EQ(
          result.events().front().type(),
          "message.sent");

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::None);

      EXPECT_TRUE(
          result.message().empty());

      EXPECT_NO_THROW(
          result.validate());
    }

    TEST(CommandResultTest, CreatesAcceptedResultWithMultipleEvents)
    {
      std::vector<RoomEvent> events;

      events.push_back(
          make_event(
              "message.sent"));

      events.push_back(
          make_event(
              "message.indexed"));

      const CommandResult result =
          CommandResult::accepted(
              std::move(events));

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          result.event_count(),
          2U);

      ASSERT_EQ(
          result.events().size(),
          2U);

      EXPECT_EQ(
          result.events()[0].type(),
          "message.sent");

      EXPECT_EQ(
          result.events()[1].type(),
          "message.indexed");
    }

    TEST(CommandResultTest, ProvidesMutableEventCollection)
    {
      CommandResult result =
          CommandResult::accepted();

      result.events().push_back(
          make_event());

      EXPECT_EQ(
          result.event_count(),
          1U);

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_NO_THROW(
          result.validate());
    }

    TEST(CommandResultTest, CreatesRejectedResult)
    {
      const CommandResult result =
          CommandResult::rejected(
              ErrorCode::Unauthorized,
              "session cannot send messages");

      EXPECT_EQ(
          result.status(),
          CommandStatus::Rejected);

      EXPECT_FALSE(
          result.is_accepted());

      EXPECT_TRUE(
          result.is_rejected());

      EXPECT_FALSE(
          result.is_ignored());

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::Unauthorized);

      EXPECT_EQ(
          result.message(),
          "session cannot send messages");

      EXPECT_TRUE(
          result.events().empty());

      EXPECT_EQ(
          result.event_count(),
          0U);

      EXPECT_NO_THROW(
          result.validate());
    }

    TEST(CommandResultTest, RejectsRejectedResultWithoutErrorCode)
    {
      EXPECT_THROW(
          static_cast<void>(
              CommandResult::rejected(
                  ErrorCode::None,
                  "command rejected")),
          Error);
    }

    TEST(CommandResultTest, CreatesIgnoredResult)
    {
      const CommandResult result =
          CommandResult::ignored();

      EXPECT_EQ(
          result.status(),
          CommandStatus::Ignored);

      EXPECT_FALSE(
          result.is_accepted());

      EXPECT_FALSE(
          result.is_rejected());

      EXPECT_TRUE(
          result.is_ignored());

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::None);

      EXPECT_TRUE(
          result.message().empty());

      EXPECT_TRUE(
          result.events().empty());

      EXPECT_EQ(
          result.event_count(),
          0U);

      EXPECT_NO_THROW(
          result.validate());
    }

    TEST(CommandResultTest, RejectedResultCannotContainEvents)
    {
      CommandResult result =
          CommandResult::rejected(
              ErrorCode::CommandRejected,
              "command rejected");

      result.events().push_back(
          make_event());

      EXPECT_FALSE(
          result.is_valid());

      EXPECT_THROW(
          result.validate(),
          Error);
    }

    TEST(CommandResultTest, IgnoredResultCannotContainEvents)
    {
      CommandResult result =
          CommandResult::ignored();

      result.events().push_back(
          make_event());

      EXPECT_FALSE(
          result.is_valid());

      EXPECT_THROW(
          result.validate(),
          Error);
    }

  } // namespace

} // namespace vix::realtime
