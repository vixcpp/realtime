/**
 *
 * @file protocol_roundtrip_test.cpp
 * @author Gaspard Kirira
 * @brief Round-trip tests for Vix Realtime protocol envelopes.
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

      payload.set_i64(
          "sequence",
          7);

      return payload;
    }

    [[nodiscard]] JsonObject make_metadata()
    {
      JsonObject metadata;

      metadata.set_string(
          "transport",
          "websocket");

      metadata.set_i64(
          "attempt",
          2);

      return metadata;
    }

    void expect_json_equal(
        const JsonObject &left,
        const JsonObject &right)
    {
      EXPECT_EQ(
          vix::json::to_json(left),
          vix::json::to_json(right));
    }

    TEST(ProtocolRoundtripTest, PreservesCommandEnvelope)
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{
              "message.send"},
          make_payload(),
          RequestId{
              "request-42"}};

      const Timestamp createdAt{
          std::chrono::seconds{
              1234}};

      command
          .set_correlation_id(
              CorrelationId{
                  "correlation-84"})
          .set_expected_version(
              RoomVersion{
                  VersionValue{12}})
          .set_created_at(
              createdAt)
          .set_metadata(
              make_metadata());

      Envelope original =
          from_command(command);

      original.set_message_id(
          "message-42");

      const Envelope parsed =
          parse(
              serialize(
                  original));

      EXPECT_EQ(
          parsed.version(),
          original.version());

      EXPECT_EQ(
          parsed.kind(),
          MessageKind::Request);

      EXPECT_EQ(
          parsed.message_id(),
          "message-42");

      EXPECT_EQ(
          parsed.type(),
          "message.send");

      EXPECT_EQ(
          parsed.request_id(),
          "request-42");

      EXPECT_EQ(
          parsed.correlation_id(),
          "correlation-84");

      EXPECT_EQ(
          parsed.room_id(),
          original.room_id());

      EXPECT_EQ(
          parsed.session_id(),
          original.session_id());

      EXPECT_EQ(
          parsed.room_version(),
          original.room_version());

      EXPECT_EQ(
          parsed.event_id(),
          original.event_id());

      EXPECT_EQ(
          parsed.created_at(),
          createdAt);

      expect_json_equal(
          parsed.payload(),
          original.payload());

      expect_json_equal(
          parsed.metadata(),
          original.metadata());

      EXPECT_NO_THROW(
          parsed.validate());
    }

    TEST(ProtocolRoundtripTest, PreservesRoomEventEnvelope)
    {
      RoomEvent event{
          make_room_id(),
          std::string{
              "message.sent"},
          make_payload(),
          EventAudience::Room};

      const Timestamp createdAt{
          std::chrono::seconds{
              2345}};

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
              SchemaVersion{3})
          .set_created_at(
              createdAt)
          .set_metadata(
              make_metadata());

      Envelope original =
          from_event(event);

      original.set_message_id(
          "event-message-12");

      const Envelope parsed =
          parse(
              serialize(
                  original));

      EXPECT_EQ(
          parsed.version(),
          original.version());

      EXPECT_EQ(
          parsed.kind(),
          MessageKind::Event);

      EXPECT_EQ(
          parsed.message_id(),
          "event-message-12");

      EXPECT_EQ(
          parsed.type(),
          "message.sent");

      EXPECT_EQ(
          parsed.request_id(),
          "request-42");

      EXPECT_EQ(
          parsed.correlation_id(),
          "correlation-84");

      EXPECT_EQ(
          parsed.room_id(),
          original.room_id());

      EXPECT_EQ(
          parsed.session_id(),
          original.session_id());

      EXPECT_EQ(
          parsed.room_version(),
          original.room_version());

      EXPECT_EQ(
          parsed.event_id(),
          original.event_id());

      EXPECT_EQ(
          parsed.created_at(),
          createdAt);

      expect_json_equal(
          parsed.payload(),
          original.payload());

      expect_json_equal(
          parsed.metadata(),
          original.metadata());

      EXPECT_NO_THROW(
          parsed.validate());
    }

    TEST(ProtocolRoundtripTest, PreservesSnapshotEnvelope)
    {
      JsonObject state;

      state.set_i64(
          "message_count",
          12);

      state.set_string(
          "topic",
          "General");

      RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{12}},
          EventId{
              EventIdValue{12}},
          std::move(state),
          SchemaVersion{2}};

      const Timestamp createdAt{
          std::chrono::seconds{
              3456}};

      snapshot
          .set_created_at(
              createdAt)
          .set_checksum(
              "sha256:abcdef")
          .set_metadata(
              make_metadata());

      Envelope original =
          from_snapshot(snapshot);

      original
          .set_message_id(
              "snapshot-message-12")
          .set_request_id(
              RequestId{
                  "request-42"})
          .set_correlation_id(
              CorrelationId{
                  "correlation-84"});

      const Envelope parsed =
          parse(
              serialize(
                  original));

      EXPECT_EQ(
          parsed.version(),
          original.version());

      EXPECT_EQ(
          parsed.kind(),
          MessageKind::Snapshot);

      EXPECT_EQ(
          parsed.message_id(),
          "snapshot-message-12");

      EXPECT_EQ(
          parsed.request_id(),
          "request-42");

      EXPECT_EQ(
          parsed.correlation_id(),
          "correlation-84");

      EXPECT_EQ(
          parsed.room_id(),
          original.room_id());

      EXPECT_EQ(
          parsed.room_version(),
          original.room_version());

      EXPECT_EQ(
          parsed.event_id(),
          original.event_id());

      EXPECT_EQ(
          parsed.created_at(),
          createdAt);

      expect_json_equal(
          parsed.payload(),
          original.payload());

      expect_json_equal(
          parsed.metadata(),
          original.metadata());

      EXPECT_NO_THROW(
          parsed.validate());
    }

    TEST(ProtocolRoundtripTest, PreservesErrorEnvelope)
    {
      Envelope original =
          make_error(
              ErrorCode::Unauthorized,
              "session is unauthorized",
              RequestId{
                  "request-42"},
              CorrelationId{
                  "correlation-84"});

      original.set_message_id(
          "error-message-42");

      const Envelope parsed =
          parse(
              serialize(
                  original));

      EXPECT_EQ(
          parsed.version(),
          original.version());

      EXPECT_EQ(
          parsed.kind(),
          MessageKind::Error);

      EXPECT_EQ(
          parsed.message_id(),
          "error-message-42");

      EXPECT_EQ(
          parsed.request_id(),
          "request-42");

      EXPECT_EQ(
          parsed.correlation_id(),
          "correlation-84");

      expect_json_equal(
          parsed.payload(),
          original.payload());

      expect_json_equal(
          parsed.metadata(),
          original.metadata());

      EXPECT_NO_THROW(
          parsed.validate());
    }

    TEST(ProtocolRoundtripTest, PreservesNestedPayload)
    {
      JsonObject author;

      author.set_string(
          "name",
          "Gaspard");

      author.set_i64(
          "id",
          42);

      vix::json::array_t tags;

      tags.push_back(
          vix::json::token{
              std::string{
                  "realtime"}});

      tags.push_back(
          vix::json::token{
              std::string{
                  "vix"}});

      JsonObject payload;

      payload.set(
          "author",
          vix::json::token{
              std::move(author)});

      payload.set(
          "tags",
          vix::json::token{
              std::move(tags)});

      payload.set_i64(
          "sequence",
          7);

      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{
              "message.send"},
          std::move(payload)};

      const Envelope original =
          from_command(command);

      const Envelope parsed =
          parse(
              serialize(
                  original));

      expect_json_equal(
          parsed.payload(),
          original.payload());

      const auto json =
          vix::json::to_json(
              parsed.payload());

      EXPECT_EQ(
          json.at("author")
              .at("name")
              .get<std::string>(),
          "Gaspard");

      EXPECT_EQ(
          json.at("tags")
              .at(0)
              .get<std::string>(),
          "realtime");

      EXPECT_EQ(
          json.at("tags")
              .at(1)
              .get<std::string>(),
          "vix");
    }

    TEST(ProtocolRoundtripTest, SerializationIsDeterministic)
    {
      const Envelope envelope =
          from_command(
              RoomCommand{
                  make_room_id(),
                  make_session_id(),
                  std::string{
                      "message.send"},
                  make_payload(),
                  RequestId{
                      "request-42"}});

      const std::string first =
          serialize(envelope);

      const std::string second =
          serialize(envelope);

      EXPECT_EQ(
          first,
          second);
    }

    TEST(ProtocolRoundtripTest, RepeatedRoundtripPreservesWireRepresentation)
    {
      const Envelope original =
          from_event(
              []()
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
                    .set_schema_version(
                        SchemaVersion{1});

                return event;
              }());

      const std::string first =
          serialize(original);

      const Envelope parsed =
          parse(first);

      const std::string second =
          serialize(parsed);

      EXPECT_EQ(
          second,
          first);
    }

  } // namespace

} // namespace vix::realtime::protocol
