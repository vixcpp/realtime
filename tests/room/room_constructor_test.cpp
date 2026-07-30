/**
 *
 * @file room_constructor_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime room construction.
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
#include <string_view>
#include <utility>

#include <vix/json/json.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/event_store.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/snapshot_store.hpp>
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
        if (event.type() !=
            "counter.incremented")
        {
          return;
        }

        const auto payload =
            vix::json::to_json(
                event.payload());

        value_ +=
            payload.at("amount")
                .get<std::int64_t>();
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
          return;
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

    [[nodiscard]] RoomId make_room_id()
    {
      return RoomId{
          std::string_view{
              "counter/main"}};
    }

    [[nodiscard]] std::shared_ptr<MemoryEventStore>
    make_event_store()
    {
      return std::make_shared<
          MemoryEventStore>();
    }

    [[nodiscard]] std::shared_ptr<MemorySnapshotStore>
    make_snapshot_store()
    {
      return std::make_shared<
          MemorySnapshotStore>();
    }

    [[nodiscard]] std::unique_ptr<Room>
    make_room(
        Config config = {})
    {
      return std::make_unique<Room>(
          make_room_id(),
          std::make_unique<CounterState>(
              42),
          std::make_unique<CounterHandler>(),
          make_event_store(),
          make_snapshot_store(),
          std::move(config));
    }

    TEST(RoomConstructorTest, ConstructsRoom)
    {
      const auto room =
          make_room();

      ASSERT_NE(
          room,
          nullptr);

      EXPECT_EQ(
          room->id(),
          make_room_id());
    }

    TEST(RoomConstructorTest, StartsInCreatedState)
    {
      const auto room =
          make_room();

      EXPECT_EQ(
          room->status(),
          RoomStatus::Created);

      EXPECT_FALSE(
          room->is_open());

      EXPECT_FALSE(
          room->is_closed());
    }

    TEST(RoomConstructorTest, StartsAtInitialStreamPosition)
    {
      const auto room =
          make_room();

      EXPECT_TRUE(
          room->version()
              .is_initial());

      EXPECT_TRUE(
          room->last_event_id()
              .empty());
    }

    TEST(RoomConstructorTest, StartsWithoutMembers)
    {
      const auto room =
          make_room();

      EXPECT_EQ(
          room->member_count(),
          0U);

      EXPECT_TRUE(
          room->empty());
    }

    TEST(RoomConstructorTest, StoresInitialRoomState)
    {
      const auto room =
          make_room();

      const auto *state =
          dynamic_cast<const CounterState *>(
              &room->state());

      ASSERT_NE(
          state,
          nullptr);

      EXPECT_EQ(
          state->value(),
          std::int64_t{42});
    }

    TEST(RoomConstructorTest, StoresConfiguration)
    {
      Config config;

      config.maxSessionsPerRoom = 64;
      config.maxPendingCommandsPerRoom = 128;
      config.snapshotEveryEvents = 25;
      config.snapshotsToKeep = 5;
      config.restoreRoomsOnOpen = false;

      const auto room =
          make_room(
              config);

      EXPECT_EQ(
          room->config()
              .maxSessionsPerRoom,
          64U);

      EXPECT_EQ(
          room->config()
              .maxPendingCommandsPerRoom,
          128U);

      EXPECT_EQ(
          room->config()
              .snapshotEveryEvents,
          25U);

      EXPECT_EQ(
          room->config()
              .snapshotsToKeep,
          5U);

      EXPECT_FALSE(
          room->config()
              .restoreRoomsOnOpen);
    }

    TEST(RoomConstructorTest, RejectsEmptyRoomIdentifier)
    {
      EXPECT_THROW(
          static_cast<void>(
              std::make_unique<Room>(
                  RoomId{},
                  std::make_unique<CounterState>(),
                  std::make_unique<CounterHandler>(),
                  make_event_store(),
                  make_snapshot_store(),
                  Config{})),
          Error);
    }

    TEST(RoomConstructorTest, RejectsNullState)
    {
      EXPECT_THROW(
          static_cast<void>(
              std::make_unique<Room>(
                  make_room_id(),
                  nullptr,
                  std::make_unique<CounterHandler>(),
                  make_event_store(),
                  make_snapshot_store(),
                  Config{})),
          Error);
    }

    TEST(RoomConstructorTest, RejectsNullHandler)
    {
      EXPECT_THROW(
          static_cast<void>(
              std::make_unique<Room>(
                  make_room_id(),
                  std::make_unique<CounterState>(),
                  nullptr,
                  make_event_store(),
                  make_snapshot_store(),
                  Config{})),
          Error);
    }

    TEST(RoomConstructorTest, RejectsNullEventStore)
    {
      EXPECT_THROW(
          static_cast<void>(
              std::make_unique<Room>(
                  make_room_id(),
                  std::make_unique<CounterState>(),
                  std::make_unique<CounterHandler>(),
                  nullptr,
                  make_snapshot_store(),
                  Config{})),
          Error);
    }

    TEST(RoomConstructorTest, RejectsNullSnapshotStore)
    {
      EXPECT_THROW(
          static_cast<void>(
              std::make_unique<Room>(
                  make_room_id(),
                  std::make_unique<CounterState>(),
                  std::make_unique<CounterHandler>(),
                  make_event_store(),
                  nullptr,
                  Config{})),
          Error);
    }

  } // namespace

} // namespace vix::realtime
