/**
 *
 * @file memory_event_store_replay_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for replaying events from the Vix Realtime memory store.
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

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/json/json.hpp>
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
        std::int64_t amount,
        std::string type = "counter.incremented")
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      RoomEvent event{
          std::move(roomId),
          std::move(type),
          std::move(payload),
          EventAudience::Room};

      event.set_room_version(
          RoomVersion{
              version});

      return event;
    }

    void append_counter_events(
        MemoryEventStore &store,
        const RoomId &roomId,
        std::size_t count)
    {
      for (std::size_t index = 0;
           index < count;
           ++index)
      {
        const auto position =
            static_cast<std::int64_t>(
                index + 1);

        store.append(
            make_event(
                roomId,
                VersionValue{
                    position},
                position));
      }
    }

    TEST(MemoryEventStoreReplayTest, LoadsAllEventsFromBeginning)
    {
      MemoryEventStore store;

      append_counter_events(
          store,
          make_room_id(),
          5);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{},
              100);

      ASSERT_EQ(
          events.size(),
          5U);

      for (std::size_t index = 0;
           index < events.size();
           ++index)
      {
        const auto expected =
            static_cast<std::int64_t>(
                index + 1);

        EXPECT_EQ(
            events[index]
                .event_id()
                .value(),
            EventIdValue{
                expected});

        EXPECT_EQ(
            events[index]
                .room_version()
                .value(),
            VersionValue{
                expected});
      }
    }

    TEST(MemoryEventStoreReplayTest, LoadsOnlyEventsAfterCursor)
    {
      MemoryEventStore store;

      append_counter_events(
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
          events[0]
              .event_id()
              .value(),
          EventIdValue{3});

      EXPECT_EQ(
          events[1]
              .event_id()
              .value(),
          EventIdValue{4});

      EXPECT_EQ(
          events[2]
              .event_id()
              .value(),
          EventIdValue{5});
    }

    TEST(MemoryEventStoreReplayTest, ExcludesCursorEvent)
    {
      MemoryEventStore store;

      append_counter_events(
          store,
          make_room_id(),
          3);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{
                  EventIdValue{1}},
              100);

      ASSERT_EQ(
          events.size(),
          2U);

      EXPECT_EQ(
          events.front()
              .event_id()
              .value(),
          EventIdValue{2});
    }

    TEST(MemoryEventStoreReplayTest, RespectsReplayLimit)
    {
      MemoryEventStore store;

      append_counter_events(
          store,
          make_room_id(),
          10);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{},
              3);

      ASSERT_EQ(
          events.size(),
          3U);

      EXPECT_EQ(
          events[0]
              .event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          events[1]
              .event_id()
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          events[2]
              .event_id()
              .value(),
          EventIdValue{3});
    }

    TEST(MemoryEventStoreReplayTest, SupportsReplayPagination)
    {
      MemoryEventStore store;

      append_counter_events(
          store,
          make_room_id(),
          7);

      const std::vector<RoomEvent> firstPage =
          store.load_after(
              make_room_id(),
              EventId{},
              3);

      ASSERT_EQ(
          firstPage.size(),
          3U);

      const EventId firstCursor =
          firstPage.back()
              .event_id();

      const std::vector<RoomEvent> secondPage =
          store.load_after(
              make_room_id(),
              firstCursor,
              3);

      ASSERT_EQ(
          secondPage.size(),
          3U);

      const EventId secondCursor =
          secondPage.back()
              .event_id();

      const std::vector<RoomEvent> thirdPage =
          store.load_after(
              make_room_id(),
              secondCursor,
              3);

      ASSERT_EQ(
          thirdPage.size(),
          1U);

      EXPECT_EQ(
          firstPage.front()
              .event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          firstPage.back()
              .event_id()
              .value(),
          EventIdValue{3});

      EXPECT_EQ(
          secondPage.front()
              .event_id()
              .value(),
          EventIdValue{4});

      EXPECT_EQ(
          secondPage.back()
              .event_id()
              .value(),
          EventIdValue{6});

      EXPECT_EQ(
          thirdPage.front()
              .event_id()
              .value(),
          EventIdValue{7});
    }

    TEST(MemoryEventStoreReplayTest, ReturnsEmptyAfterLatestEvent)
    {
      MemoryEventStore store;

      append_counter_events(
          store,
          make_room_id(),
          3);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{
                  EventIdValue{3}},
              100);

      EXPECT_TRUE(
          events.empty());
    }

    TEST(MemoryEventStoreReplayTest, ReturnsEmptyAfterFutureCursor)
    {
      MemoryEventStore store;

      append_counter_events(
          store,
          make_room_id(),
          3);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{
                  EventIdValue{100}},
              100);

      EXPECT_TRUE(
          events.empty());
    }

    TEST(MemoryEventStoreReplayTest, ReturnsEmptyForUnknownRoom)
    {
      const MemoryEventStore store;

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{},
              100);

      EXPECT_TRUE(
          events.empty());
    }

    TEST(MemoryEventStoreReplayTest, ReplaysOnlyRequestedRoom)
    {
      MemoryEventStore store;

      append_counter_events(
          store,
          make_room_id(),
          3);

      append_counter_events(
          store,
          make_other_room_id(),
          2);

      const std::vector<RoomEvent> firstRoomEvents =
          store.load_after(
              make_room_id(),
              EventId{},
              100);

      const std::vector<RoomEvent> secondRoomEvents =
          store.load_after(
              make_other_room_id(),
              EventId{},
              100);

      ASSERT_EQ(
          firstRoomEvents.size(),
          3U);

      ASSERT_EQ(
          secondRoomEvents.size(),
          2U);

      for (const RoomEvent &event :
           firstRoomEvents)
      {
        EXPECT_EQ(
            event.room_id(),
            make_room_id());
      }

      for (const RoomEvent &event :
           secondRoomEvents)
      {
        EXPECT_EQ(
            event.room_id(),
            make_other_room_id());
      }
    }

    TEST(MemoryEventStoreReplayTest, PreservesReplayOrder)
    {
      MemoryEventStore store;

      store.append(
          make_event(
              make_room_id(),
              VersionValue{1},
              10,
              "counter.incremented"));

      store.append(
          make_event(
              make_room_id(),
              VersionValue{2},
              4,
              "counter.decremented"));

      store.append(
          make_event(
              make_room_id(),
              VersionValue{3},
              0,
              "counter.reset"));

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{},
              100);

      ASSERT_EQ(
          events.size(),
          3U);

      EXPECT_EQ(
          events[0].type(),
          "counter.incremented");

      EXPECT_EQ(
          events[1].type(),
          "counter.decremented");

      EXPECT_EQ(
          events[2].type(),
          "counter.reset");
    }

    TEST(MemoryEventStoreReplayTest, PreservesReplayPayloads)
    {
      MemoryEventStore store;

      append_counter_events(
          store,
          make_room_id(),
          3);

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{},
              100);

      ASSERT_EQ(
          events.size(),
          3U);

      for (std::size_t index = 0;
           index < events.size();
           ++index)
      {
        const auto payload =
            vix::json::to_json(
                events[index].payload());

        EXPECT_EQ(
            payload.at("amount")
                .get<std::int64_t>(),
            static_cast<std::int64_t>(
                index + 1));
      }
    }

    TEST(MemoryEventStoreReplayTest, SingleAndBatchAppendsShareSequence)
    {
      MemoryEventStore store;

      store.append(
          make_event(
              make_room_id(),
              VersionValue{1},
              1));

      std::vector<RoomEvent> batch;

      batch.push_back(
          make_event(
              make_room_id(),
              VersionValue{2},
              2));

      batch.push_back(
          make_event(
              make_room_id(),
              VersionValue{3},
              3));

      store.append_batch(
          std::move(batch));

      store.append(
          make_event(
              make_room_id(),
              VersionValue{4},
              4));

      const std::vector<RoomEvent> events =
          store.load_after(
              make_room_id(),
              EventId{},
              100);

      ASSERT_EQ(
          events.size(),
          4U);

      for (std::size_t index = 0;
           index < events.size();
           ++index)
      {
        const auto expected =
            static_cast<std::int64_t>(
                index + 1);

        EXPECT_EQ(
            events[index]
                .event_id()
                .value(),
            EventIdValue{
                expected});

        EXPECT_EQ(
            events[index]
                .room_version()
                .value(),
            VersionValue{
                expected});
      }
    }

    TEST(MemoryEventStoreReplayTest, ClearRemovesReplayHistory)
    {
      MemoryEventStore store;

      append_counter_events(
          store,
          make_room_id(),
          5);

      ASSERT_TRUE(
          store.clear_room(
              make_room_id()));

      EXPECT_TRUE(
          store.load_after(
                   make_room_id(),
                   EventId{},
                   100)
              .empty());
    }

    TEST(MemoryEventStoreReplayTest, SequenceRestartsAfterClear)
    {
      MemoryEventStore store;

      append_counter_events(
          store,
          make_room_id(),
          3);

      ASSERT_TRUE(
          store.clear_room(
              make_room_id()));

      const RoomEvent stored =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{1},
                  42));

      EXPECT_EQ(
          stored.event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          stored.room_version()
              .value(),
          VersionValue{1});
    }

  } // namespace

} // namespace vix::realtime
