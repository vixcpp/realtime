/**
 *
 * @file presence_record_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime presence records.
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
#include <vix/realtime/errors.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/presence.hpp>
#include <vix/realtime/room_id.hpp>
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
              "city/river"}};
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

    [[nodiscard]] Timestamp make_joined_at()
    {
      return Timestamp{
          std::chrono::seconds{
              1000}};
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

    [[nodiscard]] Presence make_presence()
    {
      return Presence{
          make_room_id(),
          make_session_id(),
          Identity{
              "citizen-42"},
          std::optional<NodeId>{
              make_node_id()},
          ConnectionId{
              "connection-42"},
          make_joined_at(),
          make_metadata()};
    }

    TEST(PresenceRecordTest, DefaultRecordIsInvalid)
    {
      const Presence presence;

      EXPECT_FALSE(
          presence.is_valid());

      EXPECT_THROW(
          presence.validate(),
          Error);
    }

    TEST(PresenceRecordTest, ConstructsPresentRecord)
    {
      const Presence presence =
          make_presence();

      EXPECT_EQ(
          presence.room_id(),
          make_room_id());

      EXPECT_EQ(
          presence.session_id(),
          make_session_id());

      EXPECT_EQ(
          presence.identity(),
          "citizen-42");

      ASSERT_TRUE(
          presence.node_id()
              .has_value());

      EXPECT_EQ(
          *presence.node_id(),
          make_node_id());

      EXPECT_EQ(
          presence.connection_id(),
          "connection-42");

      EXPECT_EQ(
          presence.status(),
          PresenceStatus::Present);

      EXPECT_TRUE(
          presence.logically_present());

      EXPECT_TRUE(
          presence.connected());

      EXPECT_FALSE(
          presence.detached());

      EXPECT_FALSE(
          presence.left());

      EXPECT_TRUE(
          presence.is_valid());

      EXPECT_NO_THROW(
          presence.validate());
    }

    TEST(PresenceRecordTest, StoresJoinTimestamp)
    {
      const Presence presence =
          make_presence();

      EXPECT_EQ(
          presence.joined_at(),
          make_joined_at());

      EXPECT_EQ(
          presence.last_seen_at(),
          make_joined_at());

      EXPECT_FALSE(
          presence.detached_at()
              .has_value());

      EXPECT_FALSE(
          presence.left_at()
              .has_value());
    }

    TEST(PresenceRecordTest, StoresMetadata)
    {
      const Presence presence =
          make_presence();

      const auto metadata =
          vix::json::to_json(
              presence.metadata());

      EXPECT_EQ(
          metadata.at("transport")
              .get<std::string>(),
          "websocket");

      EXPECT_EQ(
          metadata.at("attempt")
              .get<std::int64_t>(),
          std::int64_t{2});
    }

    TEST(PresenceRecordTest, TouchUpdatesLastSeenTimestamp)
    {
      Presence presence =
          make_presence();

      const Timestamp touchedAt{
          std::chrono::seconds{
              1100}};

      presence.touch(
          touchedAt);

      EXPECT_EQ(
          presence.last_seen_at(),
          touchedAt);

      EXPECT_EQ(
          presence.status(),
          PresenceStatus::Present);
    }

    TEST(PresenceRecordTest, MarksRecordDetached)
    {
      Presence presence =
          make_presence();

      const Timestamp detachedAt{
          std::chrono::seconds{
              1200}};

      presence.mark_detached(
          detachedAt);

      EXPECT_EQ(
          presence.status(),
          PresenceStatus::Detached);

      EXPECT_TRUE(
          presence.logically_present());

      EXPECT_FALSE(
          presence.connected());

      EXPECT_TRUE(
          presence.detached());

      EXPECT_FALSE(
          presence.left());

      EXPECT_TRUE(
          presence.connection_id()
              .empty());

      ASSERT_TRUE(
          presence.detached_at()
              .has_value());

      EXPECT_EQ(
          *presence.detached_at(),
          detachedAt);

      EXPECT_EQ(
          presence.last_seen_at(),
          detachedAt);
    }

    TEST(PresenceRecordTest, MarksDetachedRecordPresentAgain)
    {
      Presence presence =
          make_presence();

      presence.mark_detached(
          Timestamp{
              std::chrono::seconds{
                  1200}});

      const Timestamp resumedAt{
          std::chrono::seconds{
              1300}};

      presence.mark_present(
          ConnectionId{
              "connection-84"},
          std::optional<NodeId>{
              NodeId{
                  std::string_view{
                      "node-2"}}},
          resumedAt);

      EXPECT_EQ(
          presence.status(),
          PresenceStatus::Present);

      EXPECT_TRUE(
          presence.connected());

      EXPECT_FALSE(
          presence.detached());

      EXPECT_EQ(
          presence.connection_id(),
          "connection-84");

      ASSERT_TRUE(
          presence.node_id()
              .has_value());

      EXPECT_EQ(
          presence.node_id()
              ->value(),
          "node-2");

      EXPECT_FALSE(
          presence.detached_at()
              .has_value());

      EXPECT_EQ(
          presence.last_seen_at(),
          resumedAt);
    }

    TEST(PresenceRecordTest, MarksRecordLeft)
    {
      Presence presence =
          make_presence();

      const Timestamp leftAt{
          std::chrono::seconds{
              1400}};

      presence.mark_left(
          leftAt);

      EXPECT_EQ(
          presence.status(),
          PresenceStatus::Left);

      EXPECT_FALSE(
          presence.logically_present());

      EXPECT_FALSE(
          presence.connected());

      EXPECT_FALSE(
          presence.detached());

      EXPECT_TRUE(
          presence.left());

      EXPECT_TRUE(
          presence.connection_id()
              .empty());

      ASSERT_TRUE(
          presence.left_at()
              .has_value());

      EXPECT_EQ(
          *presence.left_at(),
          leftAt);

      EXPECT_EQ(
          presence.last_seen_at(),
          leftAt);
    }

    TEST(PresenceRecordTest, LeftRecordCannotBeTouched)
    {
      Presence presence =
          make_presence();

      presence.mark_left(
          Timestamp{
              std::chrono::seconds{
                  1400}});

      EXPECT_THROW(
          presence.touch(
              Timestamp{
                  std::chrono::seconds{
                      1500}}),
          Error);
    }

    TEST(PresenceRecordTest, SetsAndClearsNodeIdentifier)
    {
      Presence presence{
          make_room_id(),
          make_session_id(),
          Identity{
              "citizen-42"},
          std::nullopt,
          ConnectionId{
              "connection-42"},
          make_joined_at(),
          {}};

      EXPECT_FALSE(
          presence.node_id()
              .has_value());

      presence.set_node_id(
          make_node_id());

      ASSERT_TRUE(
          presence.node_id()
              .has_value());

      EXPECT_EQ(
          *presence.node_id(),
          make_node_id());

      presence.clear_node_id();

      EXPECT_FALSE(
          presence.node_id()
              .has_value());
    }

    TEST(PresenceRecordTest, SetsAndClearsConnectionIdentifier)
    {
      Presence presence =
          make_presence();

      presence.set_connection_id(
          ConnectionId{
              "connection-84"});

      EXPECT_EQ(
          presence.connection_id(),
          "connection-84");

      presence.clear_connection_id();

      EXPECT_TRUE(
          presence.connection_id()
              .empty());

      EXPECT_FALSE(
          presence.connected());
    }

    TEST(PresenceRecordTest, ReplacesMetadata)
    {
      Presence presence =
          make_presence();

      JsonObject metadata;

      metadata.set_string(
          "source",
          "resume");

      presence.set_metadata(
          std::move(metadata));

      const auto stored =
          vix::json::to_json(
              presence.metadata());

      EXPECT_EQ(
          stored.at("source")
              .get<std::string>(),
          "resume");
    }

    TEST(PresenceRecordTest, ConvertsStatusesToString)
    {
      EXPECT_EQ(
          to_string(
              PresenceStatus::Present),
          std::string_view{
              "present"});

      EXPECT_EQ(
          to_string(
              PresenceStatus::Detached),
          std::string_view{
              "detached"});

      EXPECT_EQ(
          to_string(
              PresenceStatus::Left),
          std::string_view{
              "left"});
    }

  } // namespace

} // namespace vix::realtime
