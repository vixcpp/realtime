/**
 *
 * @file protocol_parse_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for parsing and serializing Vix Realtime protocol envelopes.
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
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>
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

      payload.set_i64(
          "sequence",
          7);

      return payload;
    }

    TEST(ProtocolParseTest, ParsesSerializedCommandEnvelope)
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
                  VersionValue{12}})
          .set_created_at(
              Timestamp{
                  std::chrono::seconds{
                      1234}});

      const Envelope original =
          from_command(command);

      const std::string serialized =
          serialize(original);

      const Envelope parsed =
          parse(serialized);

      EXPECT_EQ(
          parsed.kind(),
          MessageKind::Request);

      EXPECT_EQ(
          parsed.type(),
          "message.send");

      EXPECT_EQ(
          parsed.request_id(),
          "request-42");

      EXPECT_EQ(
          parsed.correlation_id(),
          "correlation-84");

      ASSERT_TRUE(
          parsed.room_id()
              .has_value());

      EXPECT_EQ(
          *parsed.room_id(),
          make_room_id());

      ASSERT_TRUE(
          parsed.session_id()
              .has_value());

      EXPECT_EQ(
          *parsed.session_id(),
          make_session_id());

      ASSERT_TRUE(
          parsed.room_version()
              .has_value());

      EXPECT_EQ(
          parsed.room_version()
              ->value(),
          VersionValue{12});

      EXPECT_FALSE(
          parsed.event_id()
              .has_value());

      const auto payload =
          vix::json::to_json(
              parsed.payload());

      EXPECT_EQ(
          payload.at("message")
              .get<std::string>(),
          "Hello");

      EXPECT_EQ(
          payload.at("sequence")
              .get<std::int64_t>(),
          std::int64_t{7});

      EXPECT_TRUE(
          parsed.is_valid());

      EXPECT_NO_THROW(
          parsed.validate());
    }

    TEST(ProtocolParseTest, ParsesSerializedEventEnvelope)
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
                  EventIdValue{21}})
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
          .set_created_at(
              Timestamp{
                  std::chrono::seconds{
                      1234}});

      const Envelope parsed =
          parse(
              serialize(
                  from_event(event)));

      EXPECT_EQ(
          parsed.kind(),
          MessageKind::Event);

      EXPECT_EQ(
          parsed.type(),
          "message.sent");

      ASSERT_TRUE(
          parsed.room_version()
              .has_value());

      EXPECT_EQ(
          parsed.room_version()
              ->value(),
          VersionValue{12});

      ASSERT_TRUE(
          parsed.event_id()
              .has_value());

      EXPECT_EQ(
          parsed.event_id()
              ->value(),
          EventIdValue{21});

      ASSERT_TRUE(
          parsed.session_id()
              .has_value());

      EXPECT_EQ(
          *parsed.session_id(),
          make_session_id());

      EXPECT_EQ(
          parsed.request_id(),
          "request-42");

      EXPECT_EQ(
          parsed.correlation_id(),
          "correlation-84");
    }

    TEST(ProtocolParseTest, PreservesNestedPayloadValues)
    {
      JsonObject nested;

      nested.set_string(
          "name",
          "Gaspard");

      JsonObject payload;

      payload.set(
          "author",
          vix::json::token{
              std::move(nested)});

      payload.set_i64(
          "count",
          42);

      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{
              "message.send"},
          std::move(payload)};

      const Envelope parsed =
          parse(
              serialize(
                  from_command(command)));

      const auto json =
          vix::json::to_json(
              parsed.payload());

      EXPECT_EQ(
          json.at("author")
              .at("name")
              .get<std::string>(),
          "Gaspard");

      EXPECT_EQ(
          json.at("count")
              .get<std::int64_t>(),
          std::int64_t{42});
    }

    TEST(ProtocolParseTest, PreservesEnvelopeMetadata)
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{
              "message.send"},
          make_payload()};

      JsonObject metadata;

      metadata.set_string(
          "transport",
          "websocket");

      metadata.set_i64(
          "attempt",
          2);

      command.set_metadata(
          std::move(metadata));

      const Envelope parsed =
          parse(
              serialize(
                  from_command(command)));

      const auto json =
          vix::json::to_json(
              parsed.metadata());

      EXPECT_EQ(
          json.at("transport")
              .get<std::string>(),
          "websocket");

      EXPECT_EQ(
          json.at("attempt")
              .get<std::int64_t>(),
          std::int64_t{2});
    }

    TEST(ProtocolParseTest, RejectsEmptyInput)
    {
      EXPECT_THROW(
          static_cast<void>(
              parse("")),
          Error);
    }

    TEST(ProtocolParseTest, RejectsMalformedJson)
    {
      EXPECT_THROW(
          static_cast<void>(
              parse("{")),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              parse(
                  R"({"kind":"request")")),
          Error);
    }

    TEST(ProtocolParseTest, RejectsJsonArray)
    {
      EXPECT_THROW(
          static_cast<void>(
              parse("[]")),
          Error);
    }

    TEST(ProtocolParseTest, RejectsJsonString)
    {
      EXPECT_THROW(
          static_cast<void>(
              parse(
                  R"("message")")),
          Error);
    }

    TEST(ProtocolParseTest, RejectsEmptyObject)
    {
      EXPECT_THROW(
          static_cast<void>(
              parse("{}")),
          Error);
    }

    TEST(ProtocolParseTest, RejectsEnvelopeWithoutType)
    {
      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{
              "message.send"},
          make_payload()};

      vix::json::Json json =
          vix::json::Json::parse(
              serialize(
                  from_command(command)));

      json.erase("type");

      EXPECT_THROW(
          static_cast<void>(
              parse(json.dump())),
          Error);
    }

    TEST(ProtocolParseTest, RejectsUnknownMessageKind)
    {
      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{
              "message.send"},
          make_payload()};

      vix::json::Json json =
          vix::json::Json::parse(
              serialize(
                  from_command(command)));

      json["kind"] =
          "unknown";

      EXPECT_THROW(
          static_cast<void>(
              parse(json.dump())),
          Error);
    }

    TEST(ProtocolParseTest, RejectsInvalidRoomIdentifier)
    {
      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{
              "message.send"},
          make_payload()};

      vix::json::Json json =
          vix::json::Json::parse(
              serialize(
                  from_command(command)));

      json["room_id"] =
          "/invalid-room";

      EXPECT_THROW(
          static_cast<void>(
              parse(json.dump())),
          Error);
    }

    TEST(ProtocolParseTest, SerializedEnvelopeIsValidJson)
    {
      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{
              "message.send"},
          make_payload()};

      const std::string serialized =
          serialize(
              from_command(command));

      EXPECT_NO_THROW(
          static_cast<void>(
              vix::json::Json::parse(
                  serialized)));
    }

  } // namespace

} // namespace vix::realtime::protocol
