
/**
 *
 * @file memory_event_store_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime in-memory event store.
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

    TEST(MemoryEventStoreTest, StartsEmpty)
    {
      const MemoryEventStore store;

      EXPECT_EQ(
          store.count(
              make_room_id()),
          0U);

      EXPECT_TRUE(
          store.latest_event_id(
                   make_room_id())
              .empty());

      EXPECT_TRUE(
          store.load_after(
                   make_room_id(),
                   EventId{},
                   100)
              .empty());
    }

    TEST(MemoryEventStoreTest, AppendsFirstEvent)
    {
      MemoryEventStore store;

      const RoomEvent stored =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{1},
                  3));

      EXPECT_EQ(
          stored.room_id(),
          make_room_id());

      EXPECT_EQ(
          stored.event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          stored.room_version()
              .value(),
          VersionValue{1});

      EXPECT_EQ(
          store.count(
              make_room_id()),
          1U);

      EXPECT_EQ(
          store.latest_event_id(
                   make_room_id())
              .value(),
          EventIdValue{1});
    }

    TEST(MemoryEventStoreTest, AssignsSequentialEventIdentifiers)
    {
      MemoryEventStore store;

      const RoomEvent first =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{1},
                  3));

      const RoomEvent second =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{2},
                  5));

      const RoomEvent third =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{3},
                  7));

      EXPECT_EQ(
          first.event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          second.event_id()
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          third.event_id()
              .value(),
          EventIdValue{3});

      EXPECT_EQ(
          store.latest_event_id(
                   make_room_id())
              .value(),
          EventIdValue{3});

      EXPECT_EQ(
          store.count(
              make_room_id()),
          3U);
    }

    TEST(MemoryEventStoreTest, PreservesEventPayload)
    {
      MemoryEventStore store;

      const RoomEvent stored =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{1},
                  42));

      const auto payload =
          vix::json::to_json(
              stored.payload());

      EXPECT_EQ(
          payload.at("amount")
              .get<std::int64_t>(),
          std::int64_t{42});
    }

    TEST(MemoryEventStoreTest, PreservesEventType)
    {
      MemoryEventStore store;

      const RoomEvent stored =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{1},
                  0,
                  "counter.reset"));

      EXPECT_EQ(
          stored.type(),
          "counter.reset");
    }

    TEST(MemoryEventStoreTest, KeepsRoomsIndependent)
    {
      MemoryEventStore store;

      const RoomEvent firstRoomEvent =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{1},
                  3));

      const RoomEvent secondRoomEvent =
          store.append(
              make_event(
                  make_other_room_id(),
                  VersionValue{1},
                  5));

      EXPECT_EQ(
          firstRoomEvent.event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          secondRoomEvent.event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          store.count(
              make_room_id()),
          1U);

      EXPECT_EQ(
          store.count(
              make_other_room_id()),
          1U);
    }

    TEST(MemoryEventStoreTest, AppendsBatchAtomically)
    {
      MemoryEventStore store;

      std::vector<RoomEvent> events;

      events.push_back(
          make_event(
              make_room_id(),
              VersionValue{1},
              3));

      events.push_back(
          make_event(
              make_room_id(),
              VersionValue{2},
              5));

      events.push_back(
          make_event(
              make_room_id(),
              VersionValue{3},
              7));

      const std::vector<RoomEvent> stored =
          store.append_batch(
              std::move(events));

      ASSERT_EQ(
          stored.size(),
          3U);

      EXPECT_EQ(
          stored[0].event_id().value(),
          EventIdValue{1});

      EXPECT_EQ(
          stored[1].event_id().value(),
          EventIdValue{2});

      EXPECT_EQ(
          stored[2].event_id().value(),
          EventIdValue{3});

      EXPECT_EQ(
          store.count(
              make_room_id()),
          3U);

      EXPECT_EQ(
          store.latest_event_id(
                   make_room_id())
              .value(),
          EventIdValue{3});
    }

    TEST(MemoryEventStoreTest, BatchContinuesExistingSequence)
    {
      MemoryEventStore store;

      const RoomEvent first =
          store.append(
              make_event(
                  make_room_id(),
                  VersionValue{1},
                  3));

      ASSERT_EQ(
          first.event_id()
              .value(),
          EventIdValue{1});

      std::vector<RoomEvent> events;

      events.push_back(
          make_event(
              make_room_id(),
              VersionValue{2},
              5));

      events.push_back(
          make_event(
              make_room_id(),
              VersionValue{3},
              7));

      const std::vector<RoomEvent> stored =
          store.append_batch(
              std::move(events));

      ASSERT_EQ(
          stored.size(),
          2U);

      EXPECT_EQ(
          stored[0].event_id().value(),
          EventIdValue{2});

      EXPECT_EQ(
          stored[1].event_id().value(),
          EventIdValue{3});
    }

    TEST(MemoryEventStoreTest, EmptyBatchDoesNotModifyStore)
    {
      MemoryEventStore store;

      const std::vector<RoomEvent> stored =
          store.append_batch({});

      EXPECT_TRUE(
          stored.empty());

      EXPECT_EQ(
          store.count(
              make_room_id()),
          0U);
    }

    TEST(MemoryEventStoreTest, RejectsNonContiguousRoomVersion)
    {
      MemoryEventStore store;

      store.append(
          make_event(
              make_room_id(),
              VersionValue{1},
              3));

      EXPECT_THROW(
          static_cast<void>(
              store.append(
                  make_event(
                      make_room_id(),
                      VersionValue{3},
                      5))),
          Error);

      EXPECT_EQ(
          store.count(
              make_room_id()),
          1U);

      EXPECT_EQ(
          store.latest_event_id(
                   make_room_id())
              .value(),
          EventIdValue{1});
    }

    TEST(MemoryEventStoreTest, FailedBatchDoesNotPersistPartialEvents)
    {
      MemoryEventStore store;

      std::vector<RoomEvent> events;

      events.push_back(
          make_event(
              make_room_id(),
              VersionValue{1},
              3));

      events.push_back(
          make_event(
              make_room_id(),
              VersionValue{3},
              5));

      EXPECT_THROW(
          static_cast<void>(
              store.append_batch(
                  std::move(events))),
          Error);

      EXPECT_EQ(
          store.count(
              make_room_id()),
          0U);

      EXPECT_TRUE(
          store.latest_event_id(
                   make_room_id())
              .empty());
    }

    TEST(MemoryEventStoreTest, ClearsExistingRoom)
    {
      MemoryEventStore store;

      store.append(
          make_event(
              make_room_id(),
              VersionValue{1},
              3));

      store.append(
          make_event(
              make_room_id(),
              VersionValue{2},
              5));

      EXPECT_TRUE(
          store.clear_room(
              make_room_id()));

      EXPECT_EQ(
          store.count(
              make_room_id()),
          0U);

      EXPECT_TRUE(
          store.latest_event_id(
                   make_room_id())
              .empty());

      EXPECT_TRUE(
          store.load_after(
                   make_room_id(),
                   EventId{},
                   100)
              .empty());
    }

    TEST(MemoryEventStoreTest, ClearingUnknownRoomReturnsFalse)
    {
      MemoryEventStore store;

      EXPECT_FALSE(
          store.clear_room(
              make_room_id()));
    }

    TEST(MemoryEventStoreTest, ClearingOneRoomDoesNotAffectOtherRooms)
    {
      MemoryEventStore store;

      store.append(
          make_event(
              make_room_id(),
              VersionValue{1},
              3));

      store.append(
          make_event(
              make_other_room_id(),
              VersionValue{1},
              5));

      EXPECT_TRUE(
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
          store.latest_event_id(
                   make_other_room_id())
              .value(),
          EventIdValue{1});
    }

  } // namespace

} // namespace vix::realtime
