/**
 *
 * @file protocol_envelope_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime protocol envelopes.
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

    TEST(ProtocolMessageKindTest, ConvertsKindsToString)
    {
      EXPECT_EQ(
          to_string(
              MessageKind::Request),
          std::string_view{
              "request"});

      EXPECT_EQ(
          to_string(
              MessageKind::Response),
          std::string_view{
              "response"});

      EXPECT_EQ(
          to_string(
              MessageKind::Event),
          std::string_view{
              "event"});

      EXPECT_EQ(
          to_string(
              MessageKind::Error),
          std::string_view{
              "error"});

      EXPECT_EQ(
          to_string(
              MessageKind::Snapshot),
          std::string_view{
              "snapshot"});

      EXPECT_EQ(
          to_string(
              MessageKind::Control),
          std::string_view{
              "control"});
    }

    TEST(ProtocolVersionTest, SupportsCurrentVersion)
    {
      constexpr Version version{
          1,
          0};

      EXPECT_TRUE(
          is_supported(version));
    }

    TEST(ProtocolVersionTest, RejectsUnsupportedMajorVersion)
    {
      constexpr Version version{
          2,
          0};

      EXPECT_FALSE(
          is_supported(version));
    }

    TEST(ProtocolEnvelopeTest, CreatesEnvelopeFromCommand)
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
                      1234}})
          .set_metadata(
              make_metadata());

      const Envelope envelope =
          from_command(command);

      EXPECT_EQ(
          envelope.kind(),
          MessageKind::Request);

      EXPECT_EQ(
          envelope.type(),
          "message.send");

      EXPECT_EQ(
          envelope.request_id(),
          "request-42");

      EXPECT_EQ(
          envelope.correlation_id(),
          "correlation-84");

      ASSERT_TRUE(
          envelope.room_id()
              .has_value());

      EXPECT_EQ(
          *envelope.room_id(),
          make_room_id());

      ASSERT_TRUE(
          envelope.session_id()
              .has_value());

      EXPECT_EQ(
          *envelope.session_id(),
          make_session_id());

      ASSERT_TRUE(
          envelope.room_version()
              .has_value());

      EXPECT_EQ(
          envelope.room_version()
              ->value(),
          VersionValue{12});

      EXPECT_FALSE(
          envelope.event_id()
              .has_value());

      EXPECT_TRUE(
          envelope.is_valid());

      EXPECT_NO_THROW(
          envelope.validate());
    }

    TEST(ProtocolEnvelopeTest, CommandEnvelopePreservesPayload)
    {
      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{
              "message.send"},
          make_payload(),
          RequestId{
              "request-42"}};

      const Envelope envelope =
          from_command(command);

      const auto payload =
          vix::json::to_json(
              envelope.payload());

      EXPECT_EQ(
          payload.at("message")
              .get<std::string>(),
          "Hello");

      EXPECT_EQ(
          payload.at("sequence")
              .get<std::int64_t>(),
          std::int64_t{7});
    }

    TEST(ProtocolEnvelopeTest, CommandEnvelopePreservesMetadata)
    {
      RoomCommand command{
          make_room_id(),
          make_session_id(),
          std::string{
              "message.send"},
          make_payload()};

      command.set_metadata(
          make_metadata());

      const Envelope envelope =
          from_command(command);

      const auto metadata =
          vix::json::to_json(
              envelope.metadata());

      EXPECT_EQ(
          metadata.at("transport")
              .get<std::string>(),
          "websocket");

      EXPECT_EQ(
          metadata.at("attempt")
              .get<std::int64_t>(),
          std::int64_t{2});
    }

    TEST(ProtocolEnvelopeTest, CreatesEnvelopeFromEvent)
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
                      1234}})
          .set_metadata(
              make_metadata());

      const Envelope envelope =
          from_event(event);

      EXPECT_EQ(
          envelope.kind(),
          MessageKind::Event);

      EXPECT_EQ(
          envelope.type(),
          "message.sent");

      ASSERT_TRUE(
          envelope.room_id()
              .has_value());

      EXPECT_EQ(
          *envelope.room_id(),
          make_room_id());

      ASSERT_TRUE(
          envelope.session_id()
              .has_value());

      EXPECT_EQ(
          *envelope.session_id(),
          make_session_id());

      ASSERT_TRUE(
          envelope.room_version()
              .has_value());

      EXPECT_EQ(
          envelope.room_version()
              ->value(),
          VersionValue{12});

      ASSERT_TRUE(
          envelope.event_id()
              .has_value());

      EXPECT_EQ(
          envelope.event_id()
              ->value(),
          EventIdValue{21});

      EXPECT_EQ(
          envelope.request_id(),
          "request-42");

      EXPECT_EQ(
          envelope.correlation_id(),
          "correlation-84");

      EXPECT_TRUE(
          envelope.is_valid());

      EXPECT_NO_THROW(
          envelope.validate());
    }

    TEST(ProtocolEnvelopeTest, CreatesEnvelopeFromSnapshot)
    {
      JsonObject state;

      state.set_i64(
          "message_count",
          42);

      RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{21}},
          EventId{
              EventIdValue{21}},
          std::move(state),
          SchemaVersion{1}};

      snapshot
          .set_created_at(
              Timestamp{
                  std::chrono::seconds{
                      1234}})
          .set_metadata(
              make_metadata());

      const Envelope envelope =
          from_snapshot(snapshot);

      EXPECT_EQ(
          envelope.kind(),
          MessageKind::Snapshot);

      ASSERT_TRUE(
          envelope.room_id()
              .has_value());

      EXPECT_EQ(
          *envelope.room_id(),
          make_room_id());

      ASSERT_TRUE(
          envelope.room_version()
              .has_value());

      EXPECT_EQ(
          envelope.room_version()
              ->value(),
          VersionValue{21});

      ASSERT_TRUE(
          envelope.event_id()
              .has_value());

      EXPECT_EQ(
          envelope.event_id()
              ->value(),
          EventIdValue{21});

      const auto payload =
          vix::json::to_json(
              envelope.payload());

      EXPECT_EQ(
          payload.at("message_count")
              .get<std::int64_t>(),
          std::int64_t{42});

      EXPECT_TRUE(
          envelope.is_valid());

      EXPECT_NO_THROW(
          envelope.validate());
    }

    TEST(ProtocolEnvelopeTest, SupportsRoutingFieldMutation)
    {
      Envelope envelope =
          from_command(
              RoomCommand{
                  make_room_id(),
                  make_session_id(),
                  std::string{
                      "message.send"},
                  make_payload()});

      envelope
          .set_message_id(
              "message-42")
          .set_request_id(
              RequestId{
                  "request-42"})
          .set_correlation_id(
              CorrelationId{
                  "correlation-84"})
          .set_room_version(
              RoomVersion{
                  VersionValue{12}})
          .set_event_id(
              EventId{
                  EventIdValue{21}});

      EXPECT_EQ(
          envelope.message_id(),
          "message-42");

      EXPECT_EQ(
          envelope.request_id(),
          "request-42");

      EXPECT_EQ(
          envelope.correlation_id(),
          "correlation-84");

      ASSERT_TRUE(
          envelope.room_version()
              .has_value());

      EXPECT_EQ(
          envelope.room_version()
              ->value(),
          VersionValue{12});

      ASSERT_TRUE(
          envelope.event_id()
              .has_value());

      EXPECT_EQ(
          envelope.event_id()
              ->value(),
          EventIdValue{21});

      envelope
          .clear_room_id()
          .clear_session_id()
          .clear_room_version()
          .clear_event_id();

      EXPECT_FALSE(
          envelope.room_id()
              .has_value());

      EXPECT_FALSE(
          envelope.session_id()
              .has_value());

      EXPECT_FALSE(
          envelope.room_version()
              .has_value());

      EXPECT_FALSE(
          envelope.event_id()
              .has_value());
    }

    TEST(ProtocolEnvelopeTest, EventEnvelopeRequiresStreamPosition)
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
                  VersionValue{12}});

      Envelope envelope =
          from_event(event);

      envelope.clear_event_id();

      EXPECT_FALSE(
          envelope.is_valid());

      EXPECT_THROW(
          envelope.validate(),
          Error);
    }

  } // namespace

} // namespace vix::realtime::protocol
