/**
 *
 * @file room_event_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime room events.
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

#include <vix/json/json.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/room_event.hpp>
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

    [[nodiscard]] SessionId make_source_session()
    {
      return SessionId{
          std::string_view{
              "session-42"}};
    }

    [[nodiscard]] SessionId make_target_session()
    {
      return SessionId{
          std::string_view{
              "session-84"}};
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

    TEST(RoomEventTest, ConstructsRoomAudienceEvent)
    {
      const RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      EXPECT_EQ(
          event.room_id(),
          make_room_id());

      EXPECT_EQ(
          event.type(),
          "message.sent");

      EXPECT_EQ(
          event.audience(),
          EventAudience::Room);

      EXPECT_TRUE(
          event.event_id().empty());

      EXPECT_TRUE(
          event.room_version().is_initial());

      EXPECT_FALSE(
          event.target_session()
              .has_value());

      EXPECT_FALSE(
          event.source_session()
              .has_value());

      EXPECT_TRUE(
          event.request_id().empty());

      EXPECT_TRUE(
          event.correlation_id().empty());

      EXPECT_EQ(
          event.schema_version(),
          SchemaVersion{1});

      EXPECT_TRUE(
          event.metadata().empty());
    }

    TEST(RoomEventTest, StoresPayload)
    {
      const RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      const auto payload =
          vix::json::to_json(
              event.payload());

      EXPECT_EQ(
          payload.at("message")
              .get<std::string>(),
          "Hello");

      EXPECT_EQ(
          payload.at("sequence")
              .get<std::int64_t>(),
          std::int64_t{7});
    }

    TEST(RoomEventTest, MutablePayloadCanBeUpdated)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      event.payload().set_string(
          "channel",
          "general");

      const auto payload =
          vix::json::to_json(
              event.payload());

      EXPECT_EQ(
          payload.at("channel")
              .get<std::string>(),
          "general");
    }

    TEST(RoomEventTest, StoresPersistentEventIdentifier)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      event.set_event_id(
          EventId{
              EventIdValue{21}});

      EXPECT_EQ(
          event.event_id().value(),
          EventIdValue{21});
    }

    TEST(RoomEventTest, StoresRoomVersion)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      event.set_room_version(
          RoomVersion{
              VersionValue{12}});

      EXPECT_EQ(
          event.room_version().value(),
          VersionValue{12});
    }

    TEST(RoomEventTest, StoresSourceSession)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      event.set_source_session(
          make_source_session());

      ASSERT_TRUE(
          event.source_session()
              .has_value());

      EXPECT_EQ(
          *event.source_session(),
          make_source_session());
    }

    TEST(RoomEventTest, StoresTargetSessionForSessionAudience)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"notification.sent"},
          make_payload(),
          EventAudience::Room};

      event.set_target_session(
          make_target_session());

      event.set_audience(
          EventAudience::Session);

      ASSERT_TRUE(
          event.target_session()
              .has_value());

      EXPECT_EQ(
          *event.target_session(),
          make_target_session());

      EXPECT_EQ(
          event.audience(),
          EventAudience::Session);

      EXPECT_NO_THROW(
          event.validate());
    }

    TEST(RoomEventTest, StoresRequestIdentifier)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      event.set_request_id(
          "request-42");

      EXPECT_EQ(
          event.request_id(),
          "request-42");
    }

    TEST(RoomEventTest, StoresCorrelationIdentifier)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      event.set_correlation_id(
          "correlation-84");

      EXPECT_EQ(
          event.correlation_id(),
          "correlation-84");
    }

    TEST(RoomEventTest, StoresSchemaVersion)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      event.set_schema_version(
          SchemaVersion{3});

      EXPECT_EQ(
          event.schema_version(),
          SchemaVersion{3});
    }

    TEST(RoomEventTest, RejectsZeroSchemaVersion)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      EXPECT_THROW(
          event.set_schema_version(
              SchemaVersion{0}),
          Error);
    }

    TEST(RoomEventTest, StoresCreationTimestamp)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      const Timestamp createdAt{
          std::chrono::seconds{
              1234}};

      event.set_created_at(
          createdAt);

      EXPECT_EQ(
          event.created_at(),
          createdAt);
    }

    TEST(RoomEventTest, StoresMetadata)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      JsonObject metadata;

      metadata.set_string(
          "source",
          "test");

      event.set_metadata(
          std::move(metadata));

      const auto storedMetadata =
          vix::json::to_json(
              event.metadata());

      EXPECT_EQ(
          storedMetadata.at("source")
              .get<std::string>(),
          "test");
    }

    TEST(RoomEventTest, AcceptsSupportedEventTypes)
    {
      EXPECT_NO_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{"message"},
                  {},
                  EventAudience::Room}));

      EXPECT_NO_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{"message.sent"},
                  {},
                  EventAudience::Room}));

      EXPECT_NO_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{"message-sent"},
                  {},
                  EventAudience::Room}));

      EXPECT_NO_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{"message_sent"},
                  {},
                  EventAudience::Room}));

      EXPECT_NO_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{"message42"},
                  {},
                  EventAudience::Room}));
    }

    TEST(RoomEventTest, AcceptsMaximumTypeLength)
    {
      const std::string type(
          RoomEvent::max_type_size,
          'a');

      const RoomEvent event{
          make_room_id(),
          type,
          {},
          EventAudience::Room};

      EXPECT_EQ(
          event.type().size(),
          RoomEvent::max_type_size);
    }

    TEST(RoomEventTest, RejectsEmptyType)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{},
                  {},
                  EventAudience::Room}),
          Error);
    }

    TEST(RoomEventTest, RejectsTypeAboveMaximumLength)
    {
      const std::string type(
          RoomEvent::max_type_size + 1,
          'a');

      EXPECT_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  type,
                  {},
                  EventAudience::Room}),
          Error);
    }

    TEST(RoomEventTest, RejectsUnsupportedTypeCharacters)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{"message/sent"},
                  {},
                  EventAudience::Room}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{"message sent"},
                  {},
                  EventAudience::Room}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{"message:sent"},
                  {},
                  EventAudience::Room}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{"message@sent"},
                  {},
                  EventAudience::Room}),
          Error);
    }

    TEST(RoomEventTest, RejectsLeadingDot)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{".message"},
                  {},
                  EventAudience::Room}),
          Error);
    }

    TEST(RoomEventTest, RejectsTrailingDot)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{"message."},
                  {},
                  EventAudience::Room}),
          Error);
    }

    TEST(RoomEventTest, RejectsConsecutiveDots)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomEvent{
                  make_room_id(),
                  std::string{"message..sent"},
                  {},
                  EventAudience::Room}),
          Error);
    }

    TEST(RoomEventTest, SessionAudienceRequiresTargetSession)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"notification.sent"},
          make_payload(),
          EventAudience::Room};

      EXPECT_THROW(
          event.set_audience(
              EventAudience::Session),
          Error);
    }

    TEST(RoomEventTest, NonSessionAudienceRejectsTargetSession)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"notification.sent"},
          make_payload(),
          EventAudience::Room};

      event.set_target_session(
          make_target_session());

      EXPECT_THROW(
          event.validate(),
          Error);
    }

    TEST(RoomEventTest, InternalAudienceAcceptsNoTarget)
    {
      const RoomEvent event{
          make_room_id(),
          std::string{"state.compacted"},
          make_payload(),
          EventAudience::Internal};

      EXPECT_EQ(
          event.audience(),
          EventAudience::Internal);

      EXPECT_FALSE(
          event.target_session()
              .has_value());

      EXPECT_NO_THROW(
          event.validate());
    }

    TEST(RoomEventTest, ValidateAcceptsCompletePersistedEvent)
    {
      RoomEvent event{
          make_room_id(),
          std::string{"message.sent"},
          make_payload(),
          EventAudience::Room};

      event
          .set_event_id(
              EventId{
                  EventIdValue{21}})
          .set_room_version(
              RoomVersion{
                  VersionValue{21}})
          .set_source_session(
              make_source_session())
          .set_request_id(
              "request-42")
          .set_correlation_id(
              "correlation-84")
          .set_schema_version(
              SchemaVersion{1});

      EXPECT_NO_THROW(
          event.validate());
    }

  } // namespace

} // namespace vix::realtime
