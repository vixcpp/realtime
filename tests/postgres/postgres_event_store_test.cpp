/** @file postgres_event_store_test.cpp */

#include <gtest/gtest.h>

#include "support.hpp"

namespace vix::realtime
{
  TEST(PostgresEventStoreTest, AppendsLoadsAndOrdersEvents)
  {
    VIX_REALTIME_REQUIRE_POSTGRES_TEST_DATABASE();
    PostgresEventStore store{postgres_test::event_options()};
    const RoomId roomId = postgres_test::room_id("postgres/events/order");
    static_cast<void>(store.clear_room(roomId));

    const RoomEvent first = store.append(postgres_test::event(roomId, 1, 3));
    const RoomEvent second = store.append(postgres_test::event(roomId, 2, 5));

    EXPECT_EQ(first.event_id().value(), EventIdValue{1});
    EXPECT_EQ(second.event_id().value(), EventIdValue{2});
    EXPECT_EQ(store.latest_event_id(roomId).value(), EventIdValue{2});

    const auto events = store.load_after(roomId, EventId{}, 10);
    ASSERT_EQ(events.size(), 2U);
    EXPECT_EQ(events[0].event_id().value(), EventIdValue{1});
    EXPECT_EQ(events[1].event_id().value(), EventIdValue{2});
    EXPECT_EQ(store.count(roomId), 2U);
  }

  TEST(PostgresEventStoreTest, RecoversEventsAfterStoreRestart)
  {
    VIX_REALTIME_REQUIRE_POSTGRES_TEST_DATABASE();
    const RoomId roomId = postgres_test::room_id("postgres/events/restart");

    {
      PostgresEventStore store{postgres_test::event_options()};
      static_cast<void>(store.clear_room(roomId));
      static_cast<void>(store.append(postgres_test::event(roomId, 1, 7)));
    }

    PostgresEventStore recovered{postgres_test::event_options()};
    const auto events = recovered.load_after(roomId, EventId{}, 10);
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events.front().event_id().value(), EventIdValue{1});
    EXPECT_EQ(recovered.latest_event_id(roomId).value(), EventIdValue{1});
  }
} // namespace vix::realtime
