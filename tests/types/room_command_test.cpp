/**
 *
 * @file room_command_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime room commands.
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
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <vix/json/json.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_version.hpp>
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
              "chat/general"}};
    }

    [[nodiscard]] SessionId make_session_id()
    {
      return SessionId{
          std::string_view{
              "session-42"}};
    }

    [[nodiscard]] JsonObject make_payload()
    {
      JsonObject payload;

      payload.set_string(
          "message",
          "Hello");

      payload.set_i64(
          "sequence",
          7);

      return payload;
    }

    TEST(RoomCommandTest, DefaultCommandIsInvalid)
    {
      const RoomCommand command;

      EXPECT_FALSE(
          command.is_valid());

      EXPECT_THROW(
          command.validate(),
          Error);
    }

    TEST(RoomCommandTest, ConstructsValidCommand)
    {
      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{"message.send"},
          make_payload()};

      EXPECT_EQ(
          command.room_id(),
          make_room_id());

      EXPECT_EQ(
          command.session_id(),
          make_session_id());

      EXPECT_EQ(
          command.type(),
          "message.send");

      EXPECT_TRUE(
          command.request_id().empty());

      EXPECT_TRUE(
          command.correlation_id().empty());

      EXPECT_FALSE(
          command.expected_version()
              .has_value());

      EXPECT_TRUE(
          command.is_valid());

      EXPECT_NO_THROW(
          command.validate());
    }

    TEST(RoomCommandTest, StoresPayload)
    {
      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{"message.send"},
          make_payload()};

      const auto payload =
          vix::json::to_json(
              command.payload());

      EXPECT_EQ(
          payload.at("message")
              .get<std::string>(),
          "Hello");

      EXPECT_EQ(
          payload.at("sequence")
              .get<std::int64_t>(),
          std::int64_t{7});
    }

    TEST(RoomCommandTest, MutablePayloadCanBeUpdated)
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{"message.send"},
          make_payload()};

      command.payload().set_string(
          "channel",
          "general");

      const auto payload =
          vix::json::to_json(
              command.payload());

      EXPECT_EQ(
          payload.at("channel")
              .get<std::string>(),
          "general");
    }

    TEST(RoomCommandTest, StoresRequestIdentifier)
    {
      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{"message.send"},
          make_payload(),
          RequestId{"request-42"}};

      EXPECT_EQ(
          command.request_id(),
          "request-42");
    }

    TEST(RoomCommandTest, SetsCorrelationIdentifier)
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{"message.send"},
          make_payload(),
          RequestId{"request-42"}};

      RoomCommand &result =
          command.set_correlation_id(
              CorrelationId{
                  "correlation-84"});

      EXPECT_EQ(
          &result,
          &command);

      EXPECT_EQ(
          command.correlation_id(),
          "correlation-84");
    }

    TEST(RoomCommandTest, SetsExpectedRoomVersion)
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{"message.send"},
          make_payload()};

      RoomCommand &result =
          command.set_expected_version(
              RoomVersion{
                  VersionValue{12}});

      EXPECT_EQ(
          &result,
          &command);

      ASSERT_TRUE(
          command.expected_version()
              .has_value());

      EXPECT_EQ(
          command.expected_version()
              ->value(),
          VersionValue{12});
    }

    TEST(RoomCommandTest, ClearsExpectedRoomVersion)
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{"message.send"},
          make_payload()};

      command.set_expected_version(
          RoomVersion{
              VersionValue{12}});

      ASSERT_TRUE(
          command.expected_version()
              .has_value());

      RoomCommand &result =
          command.clear_expected_version();

      EXPECT_EQ(
          &result,
          &command);

      EXPECT_FALSE(
          command.expected_version()
              .has_value());
    }

    TEST(RoomCommandTest, SetsCreationTimestamp)
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{"message.send"},
          make_payload()};

      const Timestamp createdAt{
          std::chrono::seconds{
              1234}};

      RoomCommand &result =
          command.set_created_at(
              createdAt);

      EXPECT_EQ(
          &result,
          &command);

      EXPECT_EQ(
          command.created_at(),
          createdAt);
    }

    TEST(RoomCommandTest, SetsMetadata)
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{"message.send"},
          make_payload()};

      JsonObject metadata;

      metadata.set_string(
          "source",
          "test");

      metadata.set_i64(
          "attempt",
          2);

      RoomCommand &result =
          command.set_metadata(
              std::move(metadata));

      EXPECT_EQ(
          &result,
          &command);

      const auto storedMetadata =
          vix::json::to_json(
              command.metadata());

      EXPECT_EQ(
          storedMetadata.at("source")
              .get<std::string>(),
          "test");

      EXPECT_EQ(
          storedMetadata.at("attempt")
              .get<std::int64_t>(),
          std::int64_t{2});
    }

    TEST(RoomCommandTest, SupportsFluentConfiguration)
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{"message.send"},
          make_payload(),
          RequestId{"request-42"}};

      JsonObject metadata;

      metadata.set_string(
          "transport",
          "websocket");

      const Timestamp createdAt{
          std::chrono::seconds{
              5678}};

      command
          .set_correlation_id(
              CorrelationId{
                  "correlation-84"})
          .set_expected_version(
              RoomVersion{
                  VersionValue{9}})
          .set_created_at(
              createdAt)
          .set_metadata(
              std::move(metadata));

      EXPECT_EQ(
          command.correlation_id(),
          "correlation-84");

      ASSERT_TRUE(
          command.expected_version()
              .has_value());

      EXPECT_EQ(
          command.expected_version()
              ->value(),
          VersionValue{9});

      EXPECT_EQ(
          command.created_at(),
          createdAt);

      EXPECT_TRUE(
          command.is_valid());
    }

    TEST(RoomCommandTest, AcceptsSupportedCommandTypes)
    {
      EXPECT_TRUE(
          RoomCommand::is_valid_type(
              "message"));

      EXPECT_TRUE(
          RoomCommand::is_valid_type(
              "message.send"));

      EXPECT_TRUE(
          RoomCommand::is_valid_type(
              "message-send"));

      EXPECT_TRUE(
          RoomCommand::is_valid_type(
              "message_send"));

      EXPECT_TRUE(
          RoomCommand::is_valid_type(
              "message42"));
    }

    TEST(RoomCommandTest, AcceptsMaximumTypeLength)
    {
      const std::string type(
          RoomCommand::max_type_size,
          'a');

      EXPECT_TRUE(
          RoomCommand::is_valid_type(
              type));

      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          type,
          {}};

      EXPECT_EQ(
          command.type().size(),
          RoomCommand::max_type_size);
    }

    TEST(RoomCommandTest, RejectsEmptyType)
    {
      EXPECT_FALSE(
          RoomCommand::is_valid_type(""));

      EXPECT_THROW(
          static_cast<void>(
              RoomCommand{
                  make_room_id(),
                  make_session_id(),
                  std::string{},
                  {}}),
          Error);
    }

    TEST(RoomCommandTest, RejectsTypeAboveMaximumLength)
    {
      const std::string type(
          RoomCommand::max_type_size + 1,
          'a');

      EXPECT_FALSE(
          RoomCommand::is_valid_type(
              type));

      EXPECT_THROW(
          static_cast<void>(
              RoomCommand{
                  make_room_id(),
                  make_session_id(),
                  type,
                  {}}),
          Error);
    }

    TEST(RoomCommandTest, RejectsUnsupportedTypeCharacters)
    {
      EXPECT_FALSE(
          RoomCommand::is_valid_type(
              "message/send"));

      EXPECT_FALSE(
          RoomCommand::is_valid_type(
              "message send"));

      EXPECT_FALSE(
          RoomCommand::is_valid_type(
              "message:send"));

      EXPECT_FALSE(
          RoomCommand::is_valid_type(
              "message@send"));

      EXPECT_THROW(
          static_cast<void>(
              RoomCommand{
                  make_room_id(),
                  make_session_id(),
                  std::string{"message/send"},
                  {}}),
          Error);
    }

    TEST(RoomCommandTest, RejectsLeadingDot)
    {
      EXPECT_FALSE(
          RoomCommand::is_valid_type(
              ".message"));

      EXPECT_THROW(
          static_cast<void>(
              RoomCommand{
                  make_room_id(),
                  make_session_id(),
                  std::string{".message"},
                  {}}),
          Error);
    }

    TEST(RoomCommandTest, RejectsTrailingDot)
    {
      EXPECT_FALSE(
          RoomCommand::is_valid_type(
              "message."));

      EXPECT_THROW(
          static_cast<void>(
              RoomCommand{
                  make_room_id(),
                  make_session_id(),
                  std::string{"message."},
                  {}}),
          Error);
    }

    TEST(RoomCommandTest, RejectsConsecutiveDots)
    {
      EXPECT_FALSE(
          RoomCommand::is_valid_type(
              "message..send"));

      EXPECT_THROW(
          static_cast<void>(
              RoomCommand{
                  make_room_id(),
                  make_session_id(),
                  std::string{"message..send"},
                  {}}),
          Error);
    }

    TEST(RoomCommandTest, ValidateAcceptsConfiguredCommand)
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{"message.send"},
          make_payload(),
          RequestId{"request-42"}};

      command
          .set_correlation_id(
              CorrelationId{
                  "correlation-84"})
          .set_expected_version(
              RoomVersion{
                  VersionValue{4}});

      EXPECT_TRUE(
          command.is_valid());

      EXPECT_NO_THROW(
          command.validate());
    }

  } // namespace

} // namespace vix::realtime
