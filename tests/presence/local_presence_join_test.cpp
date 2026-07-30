/**
 *
 * @file local_presence_join_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for joining rooms through the local presence store.
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
#include <vector>

#include <vix/json/json.hpp>
#include <vix/realtime/local_presence_store.hpp>
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

    [[nodiscard]] RoomId make_other_room_id()
    {
      return RoomId{
          std::string_view{
              "city/library"}};
    }

    [[nodiscard]] SessionId make_session_id()
    {
      return SessionId{
          std::string_view{
              "session-42"}};
    }

    [[nodiscard]] SessionId make_other_session_id()
    {
      return SessionId{
          std::string_view{
              "session-84"}};
    }

    [[nodiscard]] NodeId make_node_id()
    {
      return NodeId{
          std::string_view{
              "node-1"}};
    }

    [[nodiscard]] Presence make_presence(
        RoomId roomId,
        SessionId sessionId,
        std::string identity,
        std::string connectionId,
        std::int64_t joinedAt)
    {
      JsonObject metadata;

      metadata.set_string(
          "transport",
          "websocket");

      return Presence{
          std::move(roomId),
          std::move(sessionId),
          Identity{
              std::move(identity)},
          std::optional<NodeId>{
              make_node_id()},
          ConnectionId{
              std::move(connectionId)},
          Timestamp{
              std::chrono::seconds{
                  joinedAt}},
          std::move(metadata)};
    }

    TEST(LocalPresenceJoinTest, StartsEmpty)
    {
      const LocalPresenceStore store;

      EXPECT_EQ(
          store.count(),
          0U);

      EXPECT_EQ(
          store.count_room(
              make_room_id()),
          0U);

      EXPECT_FALSE(
          store.find(
                   make_room_id(),
                   make_session_id())
              .has_value());

      EXPECT_TRUE(
          store.list_room(
                   make_room_id())
              .empty());

      EXPECT_TRUE(
          store.list_session(
                   make_session_id())
              .empty());
    }

    TEST(LocalPresenceJoinTest, InsertsPresenceRecord)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      EXPECT_EQ(
          store.count(),
          1U);

      EXPECT_EQ(
          store.count_room(
              make_room_id()),
          1U);
    }

    TEST(LocalPresenceJoinTest, FindsJoinedPresence)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      const auto presence =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          presence.has_value());

      EXPECT_EQ(
          presence->room_id(),
          make_room_id());

      EXPECT_EQ(
          presence->session_id(),
          make_session_id());

      EXPECT_EQ(
          presence->identity(),
          "citizen-42");

      EXPECT_EQ(
          presence->connection_id(),
          "connection-42");

      EXPECT_EQ(
          presence->status(),
          PresenceStatus::Present);

      EXPECT_TRUE(
          presence->connected());
    }

    TEST(LocalPresenceJoinTest, ListsRoomPresence)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      store.upsert(
          make_presence(
              make_room_id(),
              make_other_session_id(),
              "citizen-84",
              "connection-84",
              1001));

      const std::vector<Presence> presences =
          store.list_room(
              make_room_id());

      ASSERT_EQ(
          presences.size(),
          2U);

      EXPECT_EQ(
          store.count_room(
              make_room_id()),
          2U);

      for (const Presence &presence :
           presences)
      {
        EXPECT_EQ(
            presence.room_id(),
            make_room_id());

        EXPECT_EQ(
            presence.status(),
            PresenceStatus::Present);
      }
    }

    TEST(LocalPresenceJoinTest, ListsRoomsForSession)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      store.upsert(
          make_presence(
              make_other_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1001));

      const std::vector<Presence> presences =
          store.list_session(
              make_session_id());

      ASSERT_EQ(
          presences.size(),
          2U);

      for (const Presence &presence :
           presences)
      {
        EXPECT_EQ(
            presence.session_id(),
            make_session_id());
      }
    }

    TEST(LocalPresenceJoinTest, SameSessionCanJoinMultipleRooms)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      store.upsert(
          make_presence(
              make_other_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1001));

      EXPECT_EQ(
          store.count(),
          2U);

      EXPECT_EQ(
          store.count_room(
              make_room_id()),
          1U);

      EXPECT_EQ(
          store.count_room(
              make_other_room_id()),
          1U);

      EXPECT_TRUE(
          store.find(
                   make_room_id(),
                   make_session_id())
              .has_value());

      EXPECT_TRUE(
          store.find(
                   make_other_room_id(),
                   make_session_id())
              .has_value());
    }

    TEST(LocalPresenceJoinTest, MultipleSessionsCanJoinSameRoom)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      store.upsert(
          make_presence(
              make_room_id(),
              make_other_session_id(),
              "citizen-84",
              "connection-84",
              1001));

      EXPECT_EQ(
          store.count(),
          2U);

      EXPECT_EQ(
          store.count_room(
              make_room_id()),
          2U);

      EXPECT_TRUE(
          store.find(
                   make_room_id(),
                   make_session_id())
              .has_value());

      EXPECT_TRUE(
          store.find(
                   make_room_id(),
                   make_other_session_id())
              .has_value());
    }

    TEST(LocalPresenceJoinTest, UpsertReplacesSameRoomSessionRecord)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      Presence replacement =
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-84",
              1100);

      JsonObject metadata;

      metadata.set_string(
          "source",
          "resume");

      replacement.set_metadata(
          std::move(metadata));

      store.upsert(
          std::move(replacement));

      EXPECT_EQ(
          store.count(),
          1U);

      EXPECT_EQ(
          store.count_room(
              make_room_id()),
          1U);

      const auto stored =
          store.find(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(
          stored.has_value());

      EXPECT_EQ(
          stored->connection_id(),
          "connection-84");

      EXPECT_EQ(
          stored->joined_at(),
          Timestamp{
              std::chrono::seconds{
                  1100}});

      const auto storedMetadata =
          vix::json::to_json(
              stored->metadata());

      EXPECT_EQ(
          storedMetadata.at("source")
              .get<std::string>(),
          "resume");
    }

    TEST(LocalPresenceJoinTest, KeepsRoomRecordsIndependent)
    {
      LocalPresenceStore store;

      store.upsert(
          make_presence(
              make_room_id(),
              make_session_id(),
              "citizen-42",
              "connection-42",
              1000));

      store.upsert(
          make_presence(
              make_other_room_id(),
              make_other_session_id(),
              "citizen-84",
              "connection-84",
              1001));

      const auto river =
          store.list_room(
              make_room_id());

      const auto library =
          store.list_room(
              make_other_room_id());

      ASSERT_EQ(
          river.size(),
          1U);

      ASSERT_EQ(
          library.size(),
          1U);

      EXPECT_EQ(
          river.front()
              .session_id(),
          make_session_id());

      EXPECT_EQ(
          library.front()
              .session_id(),
          make_other_session_id());
    }

  } // namespace

} // namespace vix::realtime
