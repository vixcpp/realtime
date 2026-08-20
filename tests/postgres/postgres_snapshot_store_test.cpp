/** @file postgres_snapshot_store_test.cpp */

#include <gtest/gtest.h>

#include "support.hpp"

namespace vix::realtime
{
  TEST(PostgresSnapshotStoreTest, SavesAndLoadsLatestSnapshot)
  {
    VIX_REALTIME_REQUIRE_POSTGRES_TEST_DATABASE();
    PostgresSnapshotStore store{postgres_test::snapshot_options()};
    const RoomId roomId = postgres_test::room_id("postgres/snapshots/latest");
    static_cast<void>(store.clear_room(roomId));

    static_cast<void>(store.save(postgres_test::snapshot(roomId, 1, 1, 4)));
    static_cast<void>(store.save(postgres_test::snapshot(roomId, 2, 2, 9)));

    const auto latest = store.load_latest(roomId);
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->room_version().value(), VersionValue{2});
    EXPECT_EQ(latest->last_event_id().value(), EventIdValue{2});
    EXPECT_EQ(
        vix::json::to_json(latest->state()).at("value").get<std::int64_t>(),
        std::int64_t{9});
  }

  TEST(PostgresSnapshotStoreTest, RecoversSnapshotAfterStoreRestart)
  {
    VIX_REALTIME_REQUIRE_POSTGRES_TEST_DATABASE();
    const RoomId roomId = postgres_test::room_id("postgres/snapshots/restart");

    {
      PostgresSnapshotStore store{postgres_test::snapshot_options()};
      static_cast<void>(store.clear_room(roomId));
      static_cast<void>(store.save(postgres_test::snapshot(roomId, 3, 3, 12)));
    }

    PostgresSnapshotStore recovered{postgres_test::snapshot_options()};
    const auto latest = recovered.load_latest(roomId);
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->room_version().value(), VersionValue{3});
    EXPECT_EQ(latest->last_event_id().value(), EventIdValue{3});
  }

  TEST(PostgresSnapshotStoreTest, RejectsConflictingSnapshotPosition)
  {
    VIX_REALTIME_REQUIRE_POSTGRES_TEST_DATABASE();
    PostgresSnapshotStore store{postgres_test::snapshot_options()};
    const RoomId roomId = postgres_test::room_id("postgres/snapshots/conflict");
    static_cast<void>(store.clear_room(roomId));

    static_cast<void>(store.save(postgres_test::snapshot(roomId, 2, 2, 9)));

    EXPECT_THROW(
        static_cast<void>(
            store.save(postgres_test::snapshot(roomId, 2, 1, 12))),
        Error);
  }
} // namespace vix::realtime
