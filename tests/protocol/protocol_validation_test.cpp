/**
 *
 * @file protocol_validation_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime protocol envelope validation.
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

#include <string>
#include <string_view>

#include <vix/json/json.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime::protocol
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

      return payload;
    }

    [[nodiscard]] RoomCommand make_command()
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{
              "message.send"},
          make_payload(),
          RequestId{
              "request-42"}};

      command
          .set_correlation_id(
              CorrelationId{
                  "correlation-84"})
          .set_expected_version(
              RoomVersion{
                  VersionValue{12}});

      return command;
    }

    [[nodiscard]] RoomEvent make_event()
    {
      RoomEvent event{
          make_room_id(),
          std::string{
              "message.sent"},
          make_payload(),
          EventAudience::Room};

      event
          .set_event_id(
              EventId{
                  EventIdValue{12}})
          .set_room_version(
              RoomVersion{
                  VersionValue{12}})
          .set_source_session(
              make_session_id())
          .set_request_id(
              RequestId{
                  "request-42"})
          .set_correlation_id(
              CorrelationId{
                  "correlation-84"})
          .set_schema_version(
              SchemaVersion{1});

      return event;
    }

    [[nodiscard]] RoomSnapshot make_snapshot()
    {
      JsonObject state;

      state.set_i64(
          "message_count",
          12);

      return RoomSnapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{12}},
          EventId{
              EventIdValue{12}},
          std::move(state),
          SchemaVersion{1}};
    }

    TEST(ProtocolValidationTest, AcceptsCommandEnvelope)
    {
      const Envelope envelope =
          from_command(
              make_command());

      EXPECT_NO_THROW(
          envelope.validate());
    }

    TEST(ProtocolValidationTest, AcceptsEventEnvelope)
    {
      const Envelope envelope =
          from_event(
              make_event());

      EXPECT_NO_THROW(
          envelope.validate());
    }

    TEST(ProtocolValidationTest, AcceptsSnapshotEnvelope)
    {
      const Envelope envelope =
          from_snapshot(
              make_snapshot());

      EXPECT_NO_THROW(
          envelope.validate());
    }

    TEST(ProtocolValidationTest, RejectsUnsupportedProtocolVersion)
    {
      Envelope envelope =
          from_command(
              make_command());

      envelope.set_version(
          Version{
              2,
              0});

      EXPECT_THROW(
          envelope.validate(),
          Error);
    }

    TEST(ProtocolValidationTest, RequestRequiresRoomIdentifier)
    {
      Envelope envelope =
          from_command(
              make_command());

      envelope.clear_room_id();

      EXPECT_THROW(
          envelope.validate(),
          Error);
    }

    TEST(ProtocolValidationTest, RequestRequiresSessionIdentifier)
    {
      Envelope envelope =
          from_command(
              make_command());

      envelope.clear_session_id();

      EXPECT_THROW(
          envelope.validate(),
          Error);
    }

    TEST(ProtocolValidationTest, EventRequiresRoomIdentifier)
    {
      Envelope envelope =
          from_event(
              make_event());

      envelope.clear_room_id();

      EXPECT_THROW(
          envelope.validate(),
          Error);
    }

    TEST(ProtocolValidationTest, EventRequiresRoomVersion)
    {
      Envelope envelope =
          from_event(
              make_event());

      envelope.clear_room_version();

      EXPECT_THROW(
          envelope.validate(),
          Error);
    }

    TEST(ProtocolValidationTest, EventRequiresEventIdentifier)
    {
      Envelope envelope =
          from_event(
              make_event());

      envelope.clear_event_id();

      EXPECT_THROW(
          envelope.validate(),
          Error);
    }

    TEST(ProtocolValidationTest, SnapshotRequiresRoomIdentifier)
    {
      Envelope envelope =
          from_snapshot(
              make_snapshot());

      envelope.clear_room_id();

      EXPECT_THROW(
          envelope.validate(),
          Error);
    }

    TEST(ProtocolValidationTest, SnapshotRequiresRoomVersion)
    {
      Envelope envelope =
          from_snapshot(
              make_snapshot());

      envelope.clear_room_version();

      EXPECT_THROW(
          envelope.validate(),
          Error);
    }

    TEST(ProtocolValidationTest, SnapshotRequiresEventIdentifier)
    {
      Envelope envelope =
          from_snapshot(
              make_snapshot());

      envelope.clear_event_id();

      EXPECT_THROW(
          envelope.validate(),
          Error);
    }

    TEST(ProtocolValidationTest, ParserRejectsMissingType)
    {
      vix::json::Json json =
          vix::json::Json::parse(
              serialize(
                  from_command(
                      make_command())));

      json.erase("type");

      EXPECT_THROW(
          static_cast<void>(
              parse(
                  json.dump())),
          Error);
    }

    TEST(ProtocolValidationTest, ParserRejectsEmptyType)
    {
      vix::json::Json json =
          vix::json::Json::parse(
              serialize(
                  from_command(
                      make_command())));

      json["type"] = "";

      EXPECT_THROW(
          static_cast<void>(
              parse(
                  json.dump())),
          Error);
    }

    TEST(ProtocolValidationTest, ParserRejectsUnknownKind)
    {
      vix::json::Json json =
          vix::json::Json::parse(
              serialize(
                  from_command(
                      make_command())));

      json["kind"] =
          "unknown";

      EXPECT_THROW(
          static_cast<void>(
              parse(
                  json.dump())),
          Error);
    }

    TEST(ProtocolValidationTest, ParserRejectsInvalidRoomIdentifier)
    {
      vix::json::Json json =
          vix::json::Json::parse(
              serialize(
                  from_command(
                      make_command())));

      json["room_id"] =
          "/invalid";

      EXPECT_THROW(
          static_cast<void>(
              parse(
                  json.dump())),
          Error);
    }

    TEST(ProtocolValidationTest, ParserRejectsNegativeRoomVersion)
    {
      vix::json::Json json =
          vix::json::Json::parse(
              serialize(
                  from_event(
                      make_event())));

      json["room_version"] =
          -1;

      EXPECT_THROW(
          static_cast<void>(
              parse(
                  json.dump())),
          Error);
    }

    TEST(ProtocolValidationTest, ParserRejectsNegativeEventIdentifier)
    {
      vix::json::Json json =
          vix::json::Json::parse(
              serialize(
                  from_event(
                      make_event())));

      json["event_id"] =
          -1;

      EXPECT_THROW(
          static_cast<void>(
              parse(
                  json.dump())),
          Error);
    }

    TEST(ProtocolValidationTest, CreatesValidErrorEnvelope)
    {
      const Envelope envelope =
          make_error(
              ErrorCode::InvalidCommand,
              "command is invalid",
              RequestId{
                  "request-42"},
              CorrelationId{
                  "correlation-84"});

      EXPECT_EQ(
          envelope.kind(),
          MessageKind::Error);

      EXPECT_EQ(
          envelope.request_id(),
          "request-42");

      EXPECT_EQ(
          envelope.correlation_id(),
          "correlation-84");

      EXPECT_NO_THROW(
          envelope.validate());
    }

  } // namespace

} // namespace vix::realtime::protocol
