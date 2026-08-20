/** @file postgres_event_batch_test.cpp */

#include <gtest/gtest.h>

#include "support.hpp"

namespace vix::realtime
{
  TEST(PostgresEventBatchTest, AppendsContiguousBatchAtomically)
  {
    VIX_REALTIME_REQUIRE_POSTGRES_TEST_DATABASE();
    PostgresEventStore store{postgres_test::event_options()};
    const RoomId roomId = postgres_test::room_id("postgres/events/batch");
    static_cast<void>(store.clear_room(roomId));

    const auto persisted = store.append_batch({
        postgres_test::event(roomId, 1, 2),
        postgres_test::event(roomId, 2, 4),
        postgres_test::event(roomId, 3, 6)});

    ASSERT_EQ(persisted.size(), 3U);
    EXPECT_EQ(persisted.front().event_id().value(), EventIdValue{1});
    EXPECT_EQ(persisted.back().event_id().value(), EventIdValue{3});
    EXPECT_EQ(store.count(roomId), 3U);
  }

  TEST(PostgresEventBatchTest, RejectsInvalidBatchWithoutWritingEvents)
  {
    VIX_REALTIME_REQUIRE_POSTGRES_TEST_DATABASE();
    PostgresEventStore store{postgres_test::event_options()};
    const RoomId roomId = postgres_test::room_id("postgres/events/batch-failure");
    static_cast<void>(store.clear_room(roomId));

    EXPECT_THROW(
        static_cast<void>(store.append_batch({
            postgres_test::event(roomId, 1, 2),
            postgres_test::event(roomId, 3, 6)})),
        Error);

    EXPECT_EQ(store.count(roomId), 0U);
  }
} // namespace vix::realtime
