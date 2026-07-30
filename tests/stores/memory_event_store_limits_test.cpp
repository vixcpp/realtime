/**
 *
 * @file memory_event_store_limits_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for limits and atomicity in the Realtime memory event store.
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
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>
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

    [[nodiscard]] RoomEvent make_event(
        RoomId roomId,
        VersionValue version,
        std::int64_t amount)
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      RoomEvent event{
          std::move(roomId),
          "counter.incremented",
          std::move(payload),
          EventAudience::Room};

      event.set_room_version(
          RoomVersion{
              version});

      return event;
    }

    void append_events(
        MemoryEventStore &store,
        const RoomId &roomId,
        std::size_t count)
    {
      for (std::size_t index = 0;
           index < count;
           ++index)
      {
        const auto value =
            static_cast<std::int64_t>(
                index + 1);

        store.append(
            make_event(
                roomId,
                VersionValue{
                    value},
                value));
      }
    }

    TEST(MemoryEventStoreLimitsTest, ZeroReplayLimitReturnsNoEvents)
    {
      MemoryEventStore store;

      append_events(
          store,
          make_room_id(),
          5);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{},
              0);

      EXPECT_TRUE(
          events.empty());
    }

    TEST(MemoryEventStoreLimitsTest, ReplayLimitRestrictsResultCount)
    {
      MemoryEventStore store;

      append_events(
          store,
          make_room_id(),
          10);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{},
              4);

      ASSERT_EQ(
          events.size(),
          4U);

      EXPECT_EQ(
          events.front()
              .event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          events.back()
              .event_id()
              .value(),
          EventIdValue{4});
    }

    TEST(MemoryEventStoreLimitsTest, ReplayLimitAppliesAfterCursor)
    {
      MemoryEventStore store;

      append_events(
          store,
          make_room_id(),
          10);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{
                  EventIdValue{4}},
              3);

      ASSERT_EQ(
          events.size(),
          3U);

      EXPECT_EQ(
          events[0]
              .event_id()
              .value(),
          EventIdValue{5});

      EXPECT_EQ(
          events[1]
              .event_id()
              .value(),
          EventIdValue{6});

      EXPECT_EQ(
          events[2]
              .event_id()
              .value(),
          EventIdValue{7});
    }

    TEST(MemoryEventStoreLimitsTest, LimitLargerThanHistoryReturnsAllRemainingEvents)
    {
      MemoryEventStore store;

      append_events(
          store,
          make_room_id(),
          5);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{
                  EventIdValue{2}},
              100);

      ASSERT_EQ(
          events.size(),
          3U);

      EXPECT_EQ(
          events.front()
              .event_id()
              .value(),
          EventIdValue{3});

      EXPECT_EQ(
          events.back()
              .event_id()
              .value(),
          EventIdValue{5});
    }

    TEST(MemoryEventStoreLimitsTest, MaximumLimitReturnsEntireHistory)
    {
      MemoryEventStore store;

      append_events(
          store,
          make_room_id(),
          8);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{},
              std::numeric_limits<
                  std::size_t>::max());

      ASSERT_EQ(
          events.size(),
          8U);

      EXPECT_EQ(
          events.back()
              .event_id()
              .value(),
          EventIdValue{8});
    }

    TEST(MemoryEventStoreLimitsTest, LimitDoesNotCrossRoomBoundary)
    {
      MemoryEventStore store;

      append_events(
          store,
          make_room_id(),
          5);

      append_events(
          store,
          make_other_room_id(),
          5);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{},
              100);

      ASSERT_EQ(
          events.size(),
          5U);

      for (const RoomEvent &event : events)
      {
        EXPECT_EQ(
            event.room_id(),
            make_room_id());
      }
    }

    TEST(MemoryEventStoreLimitsTest, EmptyBatchDoesNotAdvanceSequence)
    {
      MemoryEventStore store;

      const std::vector<RoomEvent> stored =
          store.append_batch({});

      EXPECT_TRUE(
          stored.empty());

      const RoomEvent first =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{1},
                  1));

      EXPECT_EQ(
          first.event_id()
              .value(),
          EventIdValue{1});
    }

    TEST(MemoryEventStoreLimitsTest, LargeValidBatchRemainsContiguous)
    {
      MemoryEventStore store;
      std::vector<RoomEvent> batch;

      constexpr std::size_t batchSize = 128;

      batch.reserve(
          batchSize);

      for (std::size_t index = 0;
           index < batchSize;
           ++index)
      {
        const auto value =
            static_cast<std::int64_t>(
                index + 1);

        batch.push_back(
            make_event(
                make_room_id(),
                VersionValue{
                    value},
                value));
      }

      const std::vector<RoomEvent> stored =
          store.append_batch(
              std::move(batch));

      ASSERT_EQ(
          stored.size(),
          batchSize);

      EXPECT_EQ(
          stored.front()
              .event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          stored.back()
              .event_id()
              .value(),
          EventIdValue{
              static_cast<EventIdValue>(
                  batchSize)});

      EXPECT_EQ(
          store.count(
              make_room_id()),
          batchSize);
    }

    TEST(MemoryEventStoreLimitsTest, InvalidBatchDoesNotPartiallyAdvanceSequence)
    {
      MemoryEventStore store;
      std::vector<RoomEvent> batch;

      batch.push_back(
          make_event(
              make_room_id(),
              VersionValue{1},
              1));

      batch.push_back(
          make_event(
              make_room_id(),
              VersionValue{2},
              2));

      batch.push_back(
          make_event(
              make_room_id(),
              VersionValue{4},
              4));

      EXPECT_THROW(
          static_cast<void>(
              store.append_batch(
                  std::move(batch))),
          Error);

      EXPECT_EQ(
          store.count(
              make_room_id()),
          0U);

      EXPECT_TRUE(
          store.latest_event_id(
                   make_room_id())
              .empty());

      const RoomEvent first =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{1},
                  1));

      EXPECT_EQ(
          first.event_id()
              .value(),
          EventIdValue{1});
    }

    TEST(MemoryEventStoreLimitsTest, FailedAppendDoesNotConsumeEventIdentifier)
    {
      MemoryEventStore store;

      store.append(
          make_event(
              make_room_id(),
              VersionValue{1},
              1));

      EXPECT_THROW(
          static_cast<void>(
              store.append(
                  make_event(
                      make_room_id(),
                      VersionValue{3},
                      3))),
          Error);

      const RoomEvent second =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{2},
                  2));

      EXPECT_EQ(
          second.event_id()
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          store.count(
              make_room_id()),
          2U);
    }

  } // namespace

} // namespace vix::realtime
