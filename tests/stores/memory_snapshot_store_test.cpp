/**
 *
 * @file memory_snapshot_store_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime in-memory snapshot store.
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

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/json/json.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
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
              "counter/main"}};
    }

    [[nodiscard]] RoomId make_other_room_id()
    {
      return RoomId{
          std::string_view{
              "counter/secondary"}};
    }

    [[nodiscard]] RoomSnapshot make_snapshot(
        RoomId roomId,
        VersionValue version,
        EventIdValue eventId,
        std::int64_t value)
    {
      JsonObject state;

      state.set_i64(
          "value",
          value);

      return RoomSnapshot{
          std::move(roomId),
          RoomVersion{
              version},
          EventId{
              eventId},
          std::move(state),
          SchemaVersion{1}};
    }

    [[nodiscard]] std::int64_t snapshot_value(
        const RoomSnapshot &snapshot)
    {
      return vix::json::to_json(
                 snapshot.state())
          .at("value")
          .get<std::int64_t>();
    }

    TEST(MemorySnapshotStoreTest, StartsEmpty)
    {
      const MemorySnapshotStore store;

      EXPECT_EQ(
          store.count(
              make_room_id()),
          0U);

      EXPECT_EQ(
          store.room_count(),
          0U);

      EXPECT_FALSE(
          store.load_latest(
                   make_room_id())
              .has_value());

      EXPECT_FALSE(
          store.load_at_or_before(
                   make_room_id(),
                   RoomVersion{
                       VersionValue{10}})
              .has_value());

      EXPECT_TRUE(
          store.load_recent(
                   make_room_id(),
                   10)
              .empty());
    }

    TEST(MemorySnapshotStoreTest, SavesSnapshot)
    {
      MemorySnapshotStore store;

      const RoomSnapshot saved =
          store.save(
              make_snapshot(
                  make_room_id(),
                  VersionValue{1},
                  EventIdValue{1},
                  10));

      EXPECT_EQ(
          saved.room_id(),
          make_room_id());

      EXPECT_EQ(
          saved.room_version()
              .value(),
          VersionValue{1});

      EXPECT_EQ(
          saved.last_event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          store.count(
              make_room_id()),
          1U);

      EXPECT_EQ(
          store.room_count(),
          1U);
    }

    TEST(MemorySnapshotStoreTest, LoadsLatestSnapshot)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{1},
              EventIdValue{1},
              10));

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{2},
              EventIdValue{2},
              20));

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{3},
              EventIdValue{3},
              30));

      const auto latest =
          store.load_latest(
              make_room_id());

      ASSERT_TRUE(
          latest.has_value());

      EXPECT_EQ(
          latest->room_version()
              .value(),
          VersionValue{3});

      EXPECT_EQ(
          latest->last_event_id()
              .value(),
          EventIdValue{3});

      EXPECT_EQ(
          snapshot_value(
              *latest),
          std::int64_t{30});
    }

    TEST(MemorySnapshotStoreTest, LoadsSnapshotAtExactVersion)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{2},
              EventIdValue{2},
              20));

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{4},
              EventIdValue{4},
              40));

      const auto snapshot =
          store.load_at_or_before(
              make_room_id(),
              RoomVersion{
                  VersionValue{4}});

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot->room_version()
              .value(),
          VersionValue{4});
    }

    TEST(MemorySnapshotStoreTest, LoadsClosestSnapshotBeforeVersion)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{2},
              EventIdValue{2},
              20));

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{4},
              EventIdValue{4},
              40));

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{8},
              EventIdValue{8},
              80));

      const auto snapshot =
          store.load_at_or_before(
              make_room_id(),
              RoomVersion{
                  VersionValue{6}});

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot->room_version()
              .value(),
          VersionValue{4});

      EXPECT_EQ(
          snapshot_value(
              *snapshot),
          std::int64_t{40});
    }

    TEST(MemorySnapshotStoreTest, ReturnsNoSnapshotBeforeFirstVersion)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{5},
              EventIdValue{5},
              50));

      EXPECT_FALSE(
          store.load_at_or_before(
                   make_room_id(),
                   RoomVersion{
                       VersionValue{4}})
              .has_value());
    }

    TEST(MemorySnapshotStoreTest, LoadsRecentSnapshotsNewestFirst)
    {
      MemorySnapshotStore store;

      for (std::int64_t value = 1;
           value <= 5;
           ++value)
      {
        store.save(
            make_snapshot(
                make_room_id(),
                VersionValue{
                    value},
                EventIdValue{
                    value},
                value * 10));
      }

      const std::vector<RoomSnapshot> recent =
          store.load_recent(
              make_room_id(),
              3);

      ASSERT_EQ(
          recent.size(),
          3U);

      EXPECT_EQ(
          recent[0]
              .room_version()
              .value(),
          VersionValue{5});

      EXPECT_EQ(
          recent[1]
              .room_version()
              .value(),
          VersionValue{4});

      EXPECT_EQ(
          recent[2]
              .room_version()
              .value(),
          VersionValue{3});
    }

    TEST(MemorySnapshotStoreTest, ZeroRecentLimitReturnsEmptyCollection)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{1},
              EventIdValue{1},
              10));

      EXPECT_TRUE(
          store.load_recent(
                   make_room_id(),
                   0)
              .empty());
    }

    TEST(MemorySnapshotStoreTest, ReplacesSameStreamPosition)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{2},
              EventIdValue{2},
              20));

      const RoomSnapshot replaced =
          store.save(
              make_snapshot(
                  make_room_id(),
                  VersionValue{2},
                  EventIdValue{2},
                  99));

      EXPECT_EQ(
          store.count(
              make_room_id()),
          1U);

      EXPECT_EQ(
          snapshot_value(
              replaced),
          std::int64_t{99});

      const auto latest =
          store.load_latest(
              make_room_id());

      ASSERT_TRUE(
          latest.has_value());

      EXPECT_EQ(
          snapshot_value(
              *latest),
          std::int64_t{99});
    }

    TEST(MemorySnapshotStoreTest, RejectsSameVersionWithDifferentEventPosition)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{2},
              EventIdValue{2},
              20));

      EXPECT_THROW(
          static_cast<void>(
              store.save(
                  make_snapshot(
                      make_room_id(),
                      VersionValue{2},
                      EventIdValue{1},
                      99))),
          Error);

      EXPECT_EQ(
          store.count(
              make_room_id()),
          1U);
    }

    TEST(MemorySnapshotStoreTest, RejectsNewVersionWithoutEventProgress)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{2},
              EventIdValue{2},
              20));

      EXPECT_THROW(
          static_cast<void>(
              store.save(
                  make_snapshot(
                      make_room_id(),
                      VersionValue{3},
                      EventIdValue{2},
                      30))),
          Error);

      EXPECT_EQ(
          store.count(
              make_room_id()),
          1U);
    }

    TEST(MemorySnapshotStoreTest, RejectsOlderSnapshotAfterNewerSnapshot)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{4},
              EventIdValue{4},
              40));

      EXPECT_THROW(
          static_cast<void>(
              store.save(
                  make_snapshot(
                      make_room_id(),
                      VersionValue{3},
                      EventIdValue{3},
                      30))),
          Error);

      EXPECT_EQ(
          store.count(
              make_room_id()),
          1U);
    }

    TEST(MemorySnapshotStoreTest, KeepsRoomsIndependent)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{1},
              EventIdValue{1},
              10));

      store.save(
          make_snapshot(
              make_other_room_id(),
              VersionValue{1},
              EventIdValue{1},
              20));

      EXPECT_EQ(
          store.count(
              make_room_id()),
          1U);

      EXPECT_EQ(
          store.count(
              make_other_room_id()),
          1U);

      EXPECT_EQ(
          store.room_count(),
          2U);
    }

    TEST(MemorySnapshotStoreTest, PrunesOldSnapshots)
    {
      MemorySnapshotStore store;

      for (std::int64_t value = 1;
           value <= 5;
           ++value)
      {
        store.save(
            make_snapshot(
                make_room_id(),
                VersionValue{
                    value},
                EventIdValue{
                    value},
                value * 10));
      }

      const std::size_t removed =
          store.prune(
              make_room_id(),
              2);

      EXPECT_EQ(
          removed,
          3U);

      EXPECT_EQ(
          store.count(
              make_room_id()),
          2U);

      const std::vector<RoomSnapshot> remaining =
          store.load_recent(
              make_room_id(),
              10);

      ASSERT_EQ(
          remaining.size(),
          2U);

      EXPECT_EQ(
          remaining[0]
              .room_version()
              .value(),
          VersionValue{5});

      EXPECT_EQ(
          remaining[1]
              .room_version()
              .value(),
          VersionValue{4});
    }

    TEST(MemorySnapshotStoreTest, PruneWithZeroRemovesRoomStream)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{1},
              EventIdValue{1},
              10));

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{2},
              EventIdValue{2},
              20));

      EXPECT_EQ(
          store.prune(
              make_room_id(),
              0),
          2U);

      EXPECT_EQ(
          store.count(
              make_room_id()),
          0U);

      EXPECT_EQ(
          store.room_count(),
          0U);
    }

    TEST(MemorySnapshotStoreTest, ClearsSingleRoom)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{1},
              EventIdValue{1},
              10));

      store.save(
          make_snapshot(
              make_other_room_id(),
              VersionValue{1},
              EventIdValue{1},
              20));

      EXPECT_TRUE(
          store.clear_room(
              make_room_id()));

      EXPECT_FALSE(
          store.clear_room(
              make_room_id()));

      EXPECT_EQ(
          store.count(
              make_room_id()),
          0U);

      EXPECT_EQ(
          store.count(
              make_other_room_id()),
          1U);

      EXPECT_EQ(
          store.room_count(),
          1U);
    }

    TEST(MemorySnapshotStoreTest, ClearsEntireStore)
    {
      MemorySnapshotStore store;

      store.save(
          make_snapshot(
              make_room_id(),
              VersionValue{1},
              EventIdValue{1},
              10));

      store.save(
          make_snapshot(
              make_other_room_id(),
              VersionValue{1},
              EventIdValue{1},
              20));

      store.clear();

      EXPECT_EQ(
          store.room_count(),
          0U);

      EXPECT_EQ(
          store.count(
              make_room_id()),
          0U);

      EXPECT_EQ(
          store.count(
              make_other_room_id()),
          0U);
    }

  } // namespace

} // namespace vix::realtime
