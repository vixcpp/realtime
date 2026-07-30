/**
 *
 * @file room_snapshot_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime room snapshots.
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
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_version.hpp>
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

    [[nodiscard]] JsonObject make_state()
    {
      JsonObject state;

      state.set_i64(
          "message_count",
          42);

      state.set_string(
          "topic",
          "General");

      return state;
    }

    TEST(RoomSnapshotTest, DefaultSnapshotIsInvalid)
    {
      const RoomSnapshot snapshot;

      EXPECT_FALSE(
          snapshot.is_valid());

      EXPECT_THROW(
          snapshot.validate(),
          Error);
    }

    TEST(RoomSnapshotTest, ConstructsValidInitialSnapshot)
    {
      const RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{},
          EventId{},
          make_state(),
          SchemaVersion{1}};

      EXPECT_EQ(
          snapshot.room_id(),
          make_room_id());

      EXPECT_TRUE(
          snapshot.room_version()
              .is_initial());

      EXPECT_TRUE(
          snapshot.last_event_id()
              .empty());

      EXPECT_EQ(
          snapshot.schema_version(),
          SchemaVersion{1});

      EXPECT_FALSE(
          snapshot.checksum()
              .has_value());

      EXPECT_TRUE(
          snapshot.metadata().empty());

      EXPECT_TRUE(
          snapshot.is_valid());

      EXPECT_NO_THROW(
          snapshot.validate());
    }

    TEST(RoomSnapshotTest, ConstructsPersistedSnapshot)
    {
      const RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{42}},
          EventId{
              EventIdValue{42}},
          make_state(),
          SchemaVersion{1}};

      EXPECT_EQ(
          snapshot.room_version()
              .value(),
          VersionValue{42});

      EXPECT_EQ(
          snapshot.last_event_id()
              .value(),
          EventIdValue{42});

      EXPECT_TRUE(
          snapshot.is_valid());
    }

    TEST(RoomSnapshotTest, StoresSerializedState)
    {
      const RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{42}},
          EventId{
              EventIdValue{42}},
          make_state(),
          SchemaVersion{1}};

      const auto state =
          vix::json::to_json(
              snapshot.state());

      EXPECT_EQ(
          state.at("message_count")
              .get<std::int64_t>(),
          std::int64_t{42});

      EXPECT_EQ(
          state.at("topic")
              .get<std::string>(),
          "General");
    }

    TEST(RoomSnapshotTest, StoresSchemaVersion)
    {
      const RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{42}},
          EventId{
              EventIdValue{42}},
          make_state(),
          SchemaVersion{3}};

      EXPECT_EQ(
          snapshot.schema_version(),
          SchemaVersion{3});
    }

    TEST(RoomSnapshotTest, RejectsZeroSchemaVersion)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomSnapshot{
                  make_room_id(),
                  RoomVersion{
                      VersionValue{42}},
                  EventId{
                      EventIdValue{42}},
                  make_state(),
                  SchemaVersion{0}}),
          Error);
    }

    TEST(RoomSnapshotTest, SetsCreationTimestamp)
    {
      RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{42}},
          EventId{
              EventIdValue{42}},
          make_state(),
          SchemaVersion{1}};

      const Timestamp createdAt{
          std::chrono::seconds{
              1234}};

      RoomSnapshot &result =
          snapshot.set_created_at(
              createdAt);

      EXPECT_EQ(
          &result,
          &snapshot);

      EXPECT_EQ(
          snapshot.created_at(),
          createdAt);
    }

    TEST(RoomSnapshotTest, SetsChecksum)
    {
      RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{42}},
          EventId{
              EventIdValue{42}},
          make_state(),
          SchemaVersion{1}};

      RoomSnapshot &result =
          snapshot.set_checksum(
              "sha256:abcdef");

      EXPECT_EQ(
          &result,
          &snapshot);

      ASSERT_TRUE(
          snapshot.checksum()
              .has_value());

      EXPECT_EQ(
          *snapshot.checksum(),
          "sha256:abcdef");
    }

    TEST(RoomSnapshotTest, ClearsChecksum)
    {
      RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{42}},
          EventId{
              EventIdValue{42}},
          make_state(),
          SchemaVersion{1}};

      snapshot.set_checksum(
          "sha256:abcdef");

      ASSERT_TRUE(
          snapshot.checksum()
              .has_value());

      RoomSnapshot &result =
          snapshot.clear_checksum();

      EXPECT_EQ(
          &result,
          &snapshot);

      EXPECT_FALSE(
          snapshot.checksum()
              .has_value());
    }

    TEST(RoomSnapshotTest, SetsMetadata)
    {
      RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{42}},
          EventId{
              EventIdValue{42}},
          make_state(),
          SchemaVersion{1}};

      JsonObject metadata;

      metadata.set_string(
          "reason",
          "event_interval");

      metadata.set_i64(
          "events_since_previous",
          100);

      RoomSnapshot &result =
          snapshot.set_metadata(
              std::move(metadata));

      EXPECT_EQ(
          &result,
          &snapshot);

      const auto storedMetadata =
          vix::json::to_json(
              snapshot.metadata());

      EXPECT_EQ(
          storedMetadata.at("reason")
              .get<std::string>(),
          "event_interval");

      EXPECT_EQ(
          storedMetadata.at(
                            "events_since_previous")
              .get<std::int64_t>(),
          std::int64_t{100});
    }

    TEST(RoomSnapshotTest, SupportsFluentConfiguration)
    {
      RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{42}},
          EventId{
              EventIdValue{42}},
          make_state(),
          SchemaVersion{1}};

      JsonObject metadata;

      metadata.set_string(
          "source",
          "test");

      const Timestamp createdAt{
          std::chrono::seconds{
              5678}};

      snapshot
          .set_created_at(
              createdAt)
          .set_checksum(
              "sha256:abcdef")
          .set_metadata(
              std::move(metadata));

      EXPECT_EQ(
          snapshot.created_at(),
          createdAt);

      ASSERT_TRUE(
          snapshot.checksum()
              .has_value());

      EXPECT_EQ(
          *snapshot.checksum(),
          "sha256:abcdef");

      EXPECT_TRUE(
          snapshot.is_valid());

      EXPECT_NO_THROW(
          snapshot.validate());
    }

    TEST(RoomSnapshotTest, RejectsRoomVersionBehindEventPosition)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomSnapshot{
                  make_room_id(),
                  RoomVersion{
                      VersionValue{4}},
                  EventId{
                      EventIdValue{5}},
                  make_state(),
                  SchemaVersion{1}}),
          Error);
    }

    TEST(RoomSnapshotTest, ValidateAcceptsMatchingStreamPosition)
    {
      const RoomSnapshot snapshot{
          make_room_id(),
          RoomVersion{
              VersionValue{100}},
          EventId{
              EventIdValue{100}},
          make_state(),
          SchemaVersion{1}};

      EXPECT_TRUE(
          snapshot.is_valid());

      EXPECT_NO_THROW(
          snapshot.validate());
    }

  } // namespace

} // namespace vix::realtime
