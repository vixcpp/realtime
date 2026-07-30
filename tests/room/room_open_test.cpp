/**
 *
 * @file room_open_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for opening and restoring Vix Realtime rooms.
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
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <vix/json/json.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    class CounterState final : public RoomState
    {
    public:
      explicit CounterState(
          std::int64_t value = 0)
          : value_(value)
      {
      }

      [[nodiscard]] SchemaVersion
      schema_version() const noexcept override
      {
        return SchemaVersion{1};
      }

      void apply(
          const RoomEvent &event) override
      {
        const auto payload =
            vix::json::to_json(
                event.payload());

        if (event.type() ==
            "counter.incremented")
        {
          value_ +=
              payload.at("amount")
                  .get<std::int64_t>();
        }
        else if (event.type() ==
                 "counter.decremented")
        {
          value_ -=
              payload.at("amount")
                  .get<std::int64_t>();
        }
      }

      [[nodiscard]] JsonObject
      serialize() const override
      {
        JsonObject state;

        state.set_i64(
            "value",
            value_);

        return state;
      }

      void restore(
          const JsonObject &state,
          SchemaVersion schemaVersion) override
      {
        if (schemaVersion !=
            SchemaVersion{1})
        {
          throw Error{
              ErrorCode::CorruptedState,
              "unsupported counter state schema"};
        }

        const auto json =
            vix::json::to_json(
                state);

        value_ =
            json.at("value")
                .get<std::int64_t>();
      }

      [[nodiscard]] std::unique_ptr<RoomState>
      clone() const override
      {
        return std::make_unique<CounterState>(
            *this);
      }

      [[nodiscard]] std::int64_t
      value() const noexcept
      {
        return value_;
      }

    private:
      std::int64_t value_{0};
    };

    class CounterHandler final : public RoomHandler
    {
    public:
      [[nodiscard]] CommandResult handle_command(
          const RoomCommand &,
          const RoomState &,
          const RoomContext &) override
      {
        return CommandResult::ignored();
      }
    };

    struct RoomDependencies
    {
      std::shared_ptr<MemoryEventStore>
          eventStore{
              std::make_shared<
                  MemoryEventStore>()};

      std::shared_ptr<MemorySnapshotStore>
          snapshotStore{
              std::make_shared<
                  MemorySnapshotStore>()};
    };

    [[nodiscard]] RoomId make_room_id()
    {
      return RoomId{
          std::string_view{
              "counter/main"}};
    }

    [[nodiscard]] RoomEvent make_event(
        VersionValue version,
        std::int64_t amount,
        std::string type =
            "counter.incremented")
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      RoomEvent event{
          make_room_id(),
          std::move(type),
          std::move(payload),
          EventAudience::Room};

      event.set_room_version(
          RoomVersion{
              version});

      return event;
    }

    [[nodiscard]] RoomSnapshot make_snapshot(
        VersionValue version,
        EventIdValue eventId,
        std::int64_t value)
    {
      JsonObject state;

      state.set_i64(
          "value",
          value);

      return RoomSnapshot{
          make_room_id(),
          RoomVersion{
              version},
          EventId{
              eventId},
          std::move(state),
          SchemaVersion{1}};
    }

    [[nodiscard]] std::unique_ptr<Room>
    make_room(
        const RoomDependencies &dependencies,
        std::int64_t initialValue = 0,
        Config config = {})
    {
      return std::make_unique<Room>(
          make_room_id(),
          std::make_unique<CounterState>(
              initialValue),
          std::make_unique<CounterHandler>(),
          dependencies.eventStore,
          dependencies.snapshotStore,
          std::move(config));
    }

    [[nodiscard]] const CounterState &
    counter_state(
        const Room &room)
    {
      const auto *state =
          dynamic_cast<const CounterState *>(
              &room.state());

      EXPECT_NE(
          state,
          nullptr);

      return *state;
    }

    TEST(RoomOpenTest, OpensFreshRoom)
    {
      const RoomDependencies dependencies;
      auto room =
          make_room(
              dependencies,
              42);

      ASSERT_EQ(
          room->status(),
          RoomStatus::Created);

      EXPECT_NO_THROW(
          room->open());

      EXPECT_EQ(
          room->status(),
          RoomStatus::Open);

      EXPECT_TRUE(
          room->is_open());

      EXPECT_FALSE(
          room->is_closed());
    }

    TEST(RoomOpenTest, FreshRoomKeepsInitialState)
    {
      const RoomDependencies dependencies;
      auto room =
          make_room(
              dependencies,
              42);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{42});

      EXPECT_TRUE(
          room->version()
              .is_initial());

      EXPECT_TRUE(
          room->last_event_id()
              .empty());
    }

    TEST(RoomOpenTest, OpeningRoomTwiceIsIdempotent)
    {
      const RoomDependencies dependencies;
      auto room =
          make_room(
              dependencies,
              42);

      room->open();

      EXPECT_NO_THROW(
          room->open());

      EXPECT_EQ(
          room->status(),
          RoomStatus::Open);

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{42});
    }

    TEST(RoomOpenTest, RestoresLatestSnapshot)
    {
      RoomDependencies dependencies;

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{2},
              EventIdValue{2},
              10));

      Config config;

      config.restoreRoomsOnOpen = true;

      auto room =
          make_room(
              dependencies,
              0,
              config);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{10});

      EXPECT_EQ(
          room->version()
              .value(),
          VersionValue{2});

      EXPECT_EQ(
          room->last_event_id()
              .value(),
          EventIdValue{2});

      EXPECT_TRUE(
          room->is_open());
    }

    TEST(RoomOpenTest, ReplaysEventsAfterLatestSnapshot)
    {
      RoomDependencies dependencies;

      const RoomEvent first =
          dependencies.eventStore->append(
              make_event(
                  VersionValue{1},
                  2));

      const RoomEvent second =
          dependencies.eventStore->append(
              make_event(
                  VersionValue{2},
                  3));

      const RoomEvent third =
          dependencies.eventStore->append(
              make_event(
                  VersionValue{3},
                  4));

      const RoomEvent fourth =
          dependencies.eventStore->append(
              make_event(
                  VersionValue{4},
                  2,
                  "counter.decremented"));

      ASSERT_EQ(
          first.event_id()
              .value(),
          EventIdValue{1});

      ASSERT_EQ(
          second.event_id()
              .value(),
          EventIdValue{2});

      ASSERT_EQ(
          third.event_id()
              .value(),
          EventIdValue{3});

      ASSERT_EQ(
          fourth.event_id()
              .value(),
          EventIdValue{4});

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{2},
              EventIdValue{2},
              5));

      Config config;

      config.restoreRoomsOnOpen = true;

      auto room =
          make_room(
              dependencies,
              0,
              config);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{7});

      EXPECT_EQ(
          room->version()
              .value(),
          VersionValue{4});

      EXPECT_EQ(
          room->last_event_id()
              .value(),
          EventIdValue{4});
    }

    TEST(RoomOpenTest, ReplaysEntireHistoryWithoutSnapshot)
    {
      RoomDependencies dependencies;

      dependencies.eventStore->append(
          make_event(
              VersionValue{1},
              5));

      dependencies.eventStore->append(
          make_event(
              VersionValue{2},
              2,
              "counter.decremented"));

      dependencies.eventStore->append(
          make_event(
              VersionValue{3},
              4));

      Config config;

      config.restoreRoomsOnOpen = true;

      auto room =
          make_room(
              dependencies,
              0,
              config);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{7});

      EXPECT_EQ(
          room->version()
              .value(),
          VersionValue{3});

      EXPECT_EQ(
          room->last_event_id()
              .value(),
          EventIdValue{3});
    }

    TEST(RoomOpenTest, DoesNotRestoreWhenRestorationIsDisabled)
    {
      RoomDependencies dependencies;

      dependencies.eventStore->append(
          make_event(
              VersionValue{1},
              5));

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{1},
              EventIdValue{1},
              5));

      Config config;

      config.restoreRoomsOnOpen = false;

      auto room =
          make_room(
              dependencies,
              42,
              config);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{42});

      EXPECT_TRUE(
          room->version()
              .is_initial());

      EXPECT_TRUE(
          room->last_event_id()
              .empty());
    }

    TEST(RoomOpenTest, FailedRestoreMarksRoomAsFailed)
    {
      RoomDependencies dependencies;

      JsonObject state;

      state.set_i64(
          "value",
          42);

      dependencies.snapshotStore->save(
          RoomSnapshot{
              make_room_id(),
              RoomVersion{
                  VersionValue{1}},
              EventId{
                  EventIdValue{1}},
              std::move(state),
              SchemaVersion{2}});

      Config config;

      config.restoreRoomsOnOpen = true;

      auto room =
          make_room(
              dependencies,
              0,
              config);

      EXPECT_THROW(
          room->open(),
          Error);

      EXPECT_EQ(
          room->status(),
          RoomStatus::Failed);

      EXPECT_FALSE(
          room->is_open());
    }

  } // namespace

} // namespace vix::realtime
