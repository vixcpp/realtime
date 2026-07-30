/**
 *
 * @file room_context_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime room execution context.
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
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <vix/json/json.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/room_context.hpp>
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

    [[nodiscard]] NodeId make_node_id()
    {
      return NodeId{
          std::string_view{
              "node-1"}};
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

    [[nodiscard]] Timestamp make_timestamp()
    {
      return Timestamp{
          std::chrono::seconds{
              1234}};
    }

    [[nodiscard]] RoomContext make_context()
    {
      return RoomContext{
          make_room_id(),
          RoomVersion{
              VersionValue{12}},
          EventId{
              EventIdValue{21}},
          std::optional<SessionId>{
              make_session_id()},
          RequestId{
              "request-42"},
          CorrelationId{
              "correlation-84"},
          std::optional<NodeId>{
              make_node_id()},
          make_timestamp(),
          make_metadata()};
    }

    TEST(RoomContextTest, ConstructsValidContext)
    {
      const RoomContext context =
          make_context();

      EXPECT_TRUE(
          context.is_valid());

      EXPECT_NO_THROW(
          context.validate());
    }

    TEST(RoomContextTest, StoresRoomIdentifier)
    {
      const RoomContext context =
          make_context();

      EXPECT_EQ(
          context.room_id(),
          make_room_id());
    }

    TEST(RoomContextTest, StoresCurrentRoomVersion)
    {
      const RoomContext context =
          make_context();

      EXPECT_EQ(
          context.room_version()
              .value(),
          VersionValue{12});
    }

    TEST(RoomContextTest, StoresLastEventIdentifier)
    {
      const RoomContext context =
          make_context();

      EXPECT_EQ(
          context.last_event_id()
              .value(),
          EventIdValue{21});
    }

    TEST(RoomContextTest, ReturnsNextRoomVersion)
    {
      const RoomContext context =
          make_context();

      const RoomVersion next =
          context.next_room_version();

      EXPECT_EQ(
          next.value(),
          VersionValue{13});

      EXPECT_EQ(
          context.room_version()
              .value(),
          VersionValue{12});
    }

    TEST(RoomContextTest, ReturnsNextEventIdentifier)
    {
      const RoomContext context =
          make_context();

      const EventId next =
          context.next_event_id();

      EXPECT_EQ(
          next.value(),
          EventIdValue{22});

      EXPECT_EQ(
          context.last_event_id()
              .value(),
          EventIdValue{21});
    }

    TEST(RoomContextTest, StoresSessionIdentifier)
    {
      const RoomContext context =
          make_context();

      ASSERT_TRUE(
          context.session_id()
              .has_value());

      EXPECT_EQ(
          *context.session_id(),
          make_session_id());
    }

    TEST(RoomContextTest, SupportsContextWithoutSession)
    {
      const RoomContext context{
          make_room_id(),
          RoomVersion{
              VersionValue{12}},
          EventId{
              EventIdValue{21}},
          std::nullopt,
          RequestId{
              "request-42"},
          CorrelationId{
              "correlation-84"},
          std::optional<NodeId>{
              make_node_id()},
          make_timestamp(),
          make_metadata()};

      EXPECT_FALSE(
          context.session_id()
              .has_value());

      EXPECT_TRUE(
          context.is_valid());

      EXPECT_NO_THROW(
          context.validate());
    }

    TEST(RoomContextTest, StoresRequestIdentifier)
    {
      const RoomContext context =
          make_context();

      EXPECT_EQ(
          context.request_id(),
          "request-42");
    }

    TEST(RoomContextTest, StoresCorrelationIdentifier)
    {
      const RoomContext context =
          make_context();

      EXPECT_EQ(
          context.correlation_id(),
          "correlation-84");
    }

    TEST(RoomContextTest, StoresNodeIdentifier)
    {
      const RoomContext context =
          make_context();

      ASSERT_TRUE(
          context.node_id()
              .has_value());

      EXPECT_EQ(
          *context.node_id(),
          make_node_id());
    }

    TEST(RoomContextTest, SupportsContextWithoutNode)
    {
      const RoomContext context{
          make_room_id(),
          RoomVersion{
              VersionValue{12}},
          EventId{
              EventIdValue{21}},
          std::optional<SessionId>{
              make_session_id()},
          RequestId{
              "request-42"},
          CorrelationId{
              "correlation-84"},
          std::nullopt,
          make_timestamp(),
          make_metadata()};

      EXPECT_FALSE(
          context.node_id()
              .has_value());

      EXPECT_TRUE(
          context.is_valid());

      EXPECT_NO_THROW(
          context.validate());
    }

    TEST(RoomContextTest, StoresServerTimestamp)
    {
      const RoomContext context =
          make_context();

      EXPECT_EQ(
          context.now(),
          make_timestamp());
    }

    TEST(RoomContextTest, StoresMetadata)
    {
      const RoomContext context =
          make_context();

      const auto metadata =
          vix::json::to_json(
              context.metadata());

      EXPECT_EQ(
          metadata.at("transport")
              .get<std::string>(),
          "websocket");

      EXPECT_EQ(
          metadata.at("attempt")
              .get<std::int64_t>(),
          std::int64_t{2});
    }

    TEST(RoomContextTest, SupportsEmptyMetadata)
    {
      const RoomContext context{
          make_room_id(),
          RoomVersion{
              VersionValue{12}},
          EventId{
              EventIdValue{21}},
          std::optional<SessionId>{
              make_session_id()},
          RequestId{
              "request-42"},
          CorrelationId{
              "correlation-84"},
          std::optional<NodeId>{
              make_node_id()},
          make_timestamp(),
          {}};

      EXPECT_TRUE(
          context.metadata().empty());

      EXPECT_TRUE(
          context.is_valid());
    }

    TEST(RoomContextTest, SupportsInitialStreamPosition)
    {
      const RoomContext context{
          make_room_id(),
          RoomVersion{},
          EventId{},
          std::optional<SessionId>{
              make_session_id()},
          RequestId{
              "request-42"},
          CorrelationId{
              "correlation-84"},
          std::optional<NodeId>{
              make_node_id()},
          make_timestamp(),
          {}};

      EXPECT_TRUE(
          context.room_version()
              .is_initial());

      EXPECT_TRUE(
          context.last_event_id()
              .empty());

      EXPECT_EQ(
          context.next_room_version()
              .value(),
          VersionValue{1});

      EXPECT_EQ(
          context.next_event_id()
              .value(),
          EventIdValue{1});

      EXPECT_TRUE(
          context.is_valid());

      EXPECT_NO_THROW(
          context.validate());
    }

  } // namespace

} // namespace vix::realtime
