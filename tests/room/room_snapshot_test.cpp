/**
 *
 * @file room_snapshot_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for snapshot creation in Vix Realtime rooms.
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

#include <concepts>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include <vix/json/json.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/errors.hpp>
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
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    template <typename>
    inline constexpr bool dependentFalse = false;

    template <typename RoomType>
    [[nodiscard]] CommandResult dispatch_command(
        RoomType &room,
        const RoomCommand &command)
    {
      if constexpr (
          requires {
            {
              room.handle_command(command)
            } -> std::same_as<CommandResult>;
          })
      {
        return room.handle_command(
            command);
      }
      else if constexpr (
          requires {
            {
              room.process_command(command)
            } -> std::same_as<CommandResult>;
          })
      {
        return room.process_command(
            command);
      }
      else if constexpr (
          requires {
            {
              room.command(command)
            } -> std::same_as<CommandResult>;
          })
      {
        return room.command(
            command);
      }
      else if constexpr (
          requires {
            {
              room.execute_command(command)
            } -> std::same_as<CommandResult>;
          })
      {
        return room.execute_command(
            command);
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room command API");
      }
    }

    template <typename RoomType>
    void trigger_snapshot(
        RoomType &room)
    {
      if constexpr (
          requires {
            room.snapshot();
          })
      {
        static_cast<void>(
            room.snapshot());
      }
      else if constexpr (
          requires {
            room.create_snapshot();
          })
      {
        static_cast<void>(
            room.create_snapshot());
      }
      else if constexpr (
          requires {
            room.save_snapshot();
          })
      {
        static_cast<void>(
            room.save_snapshot());
      }
      else if constexpr (
          requires {
            room.take_snapshot();
          })
      {
        static_cast<void>(
            room.take_snapshot());
      }
      else if constexpr (
          requires {
            room.persist_snapshot();
          })
      {
        static_cast<void>(
            room.persist_snapshot());
      }
      else if constexpr (
          requires {
            room.snapshot(true);
          })
      {
        static_cast<void>(
            room.snapshot(true));
      }
      else if constexpr (
          requires {
            room.create_snapshot(true);
          })
      {
        static_cast<void>(
            room.create_snapshot(true));
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room snapshot API");
      }
    }

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
          const RoomCommand &command,
          const RoomState &,
          const RoomContext &) override
      {
        if (command.type() !=
            "counter.increment")
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "unsupported command");
        }

        const auto payload =
            vix::json::to_json(
                command.payload());

        const std::int64_t amount =
            payload.at("amount")
                .get<std::int64_t>();

        JsonObject eventPayload;

        eventPayload.set_i64(
            "amount",
            amount);

        RoomEvent event{
            command.room_id(),
            "counter.incremented",
            std::move(eventPayload),
            EventAudience::Room};

        event
            .set_source_session(
                command.session_id())
            .set_request_id(
                command.request_id())
            .set_correlation_id(
                command.correlation_id());

        return CommandResult::accepted(
            {std::move(event)});
      }
    };

    struct RoomFixture
    {
      std::shared_ptr<MemoryEventStore>
          eventStore{
              std::make_shared<
                  MemoryEventStore>()};

      std::shared_ptr<MemorySnapshotStore>
          snapshotStore{
              std::make_shared<
                  MemorySnapshotStore>()};

      std::unique_ptr<Room> room{};
    };

    [[nodiscard]] RoomId make_room_id()
    {
      return RoomId{
          std::string_view{
              "counter/main"}};
    }

    [[nodiscard]] SessionId make_session_id()
    {
      return SessionId{
          std::string_view{
              "session-42"}};
    }

    [[nodiscard]] RoomCommand make_command(
        std::int64_t amount)
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      RoomCommand command{
          make_room_id(),
          make_session_id(),
          "counter.increment",
          std::move(payload),
          RequestId{
              "request-42"}};

      command.set_correlation_id(
          CorrelationId{
              "correlation-84"});

      return command;
    }

    [[nodiscard]] RoomFixture make_fixture(
        Config config = {},
        std::int64_t initialValue = 0)
    {
      RoomFixture fixture;

      fixture.room =
          std::make_unique<Room>(
              make_room_id(),
              std::make_unique<CounterState>(
                  initialValue),
              std::make_unique<CounterHandler>(),
              fixture.eventStore,
              fixture.snapshotStore,
              std::move(config));

      fixture.room->open();

      return fixture;
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

    [[nodiscard]] std::int64_t
    snapshot_value(
        const RoomSnapshot &snapshot)
    {
      return vix::json::to_json(
                 snapshot.state())
          .at("value")
          .get<std::int64_t>();
    }

    TEST(RoomSnapshotTest, CreatesSnapshotForCurrentRoomState)
    {
      RoomFixture fixture =
          make_fixture();

      ASSERT_TRUE(
          dispatch_command(
              *fixture.room,
              make_command(
                  5))
              .is_accepted());

      trigger_snapshot(
          *fixture.room);

      const auto snapshot =
          fixture.snapshotStore
              ->load_latest(
                  make_room_id());

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot_value(
              *snapshot),
          std::int64_t{5});
    }

    TEST(RoomSnapshotTest, SnapshotCapturesRoomIdentifier)
    {
      RoomFixture fixture =
          make_fixture();

      dispatch_command(
          *fixture.room,
          make_command(
              5));

      trigger_snapshot(
          *fixture.room);

      const auto snapshot =
          fixture.snapshotStore
              ->load_latest(
                  make_room_id());

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot->room_id(),
          make_room_id());
    }

    TEST(RoomSnapshotTest, SnapshotCapturesCurrentStreamPosition)
    {
      RoomFixture fixture =
          make_fixture();

      dispatch_command(
          *fixture.room,
          make_command(
              5));

      dispatch_command(
          *fixture.room,
          make_command(
              3));

      trigger_snapshot(
          *fixture.room);

      const auto snapshot =
          fixture.snapshotStore
              ->load_latest(
                  make_room_id());

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot->room_version()
              .value(),
          VersionValue{2});

      EXPECT_EQ(
          snapshot->last_event_id()
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          snapshot->room_version(),
          fixture.room->version());

      EXPECT_EQ(
          snapshot->last_event_id(),
          fixture.room->last_event_id());
    }

    TEST(RoomSnapshotTest, SnapshotCapturesStateSchemaVersion)
    {
      RoomFixture fixture =
          make_fixture();

      dispatch_command(
          *fixture.room,
          make_command(
              5));

      trigger_snapshot(
          *fixture.room);

      const auto snapshot =
          fixture.snapshotStore
              ->load_latest(
                  make_room_id());

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot->schema_version(),
          SchemaVersion{1});
    }

    TEST(RoomSnapshotTest, SnapshotDoesNotMutateRoom)
    {
      RoomFixture fixture =
          make_fixture(
              Config{},
              10);

      dispatch_command(
          *fixture.room,
          make_command(
              5));

      const RoomVersion versionBefore =
          fixture.room->version();

      const EventId eventIdBefore =
          fixture.room->last_event_id();

      const std::int64_t valueBefore =
          counter_state(
              *fixture.room)
              .value();

      trigger_snapshot(
          *fixture.room);

      EXPECT_EQ(
          fixture.room->version(),
          versionBefore);

      EXPECT_EQ(
          fixture.room->last_event_id(),
          eventIdBefore);

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          valueBefore);
    }

    TEST(RoomSnapshotTest, RepeatedSnapshotAtSamePositionDoesNotDuplicateHistory)
    {
      RoomFixture fixture =
          make_fixture();

      dispatch_command(
          *fixture.room,
          make_command(
              5));

      trigger_snapshot(
          *fixture.room);

      trigger_snapshot(
          *fixture.room);

      EXPECT_EQ(
          fixture.snapshotStore
              ->count(
                  make_room_id()),
          1U);

      const auto snapshot =
          fixture.snapshotStore
              ->load_latest(
                  make_room_id());

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot->room_version()
              .value(),
          VersionValue{1});
    }

    TEST(RoomSnapshotTest, CreatesNewSnapshotAfterRoomProgress)
    {
      RoomFixture fixture =
          make_fixture();

      dispatch_command(
          *fixture.room,
          make_command(
              5));

      trigger_snapshot(
          *fixture.room);

      dispatch_command(
          *fixture.room,
          make_command(
              3));

      trigger_snapshot(
          *fixture.room);

      EXPECT_EQ(
          fixture.snapshotStore
              ->count(
                  make_room_id()),
          2U);

      const auto latest =
          fixture.snapshotStore
              ->load_latest(
                  make_room_id());

      ASSERT_TRUE(
          latest.has_value());

      EXPECT_EQ(
          latest->room_version()
              .value(),
          VersionValue{2});

      EXPECT_EQ(
          latest->last_event_id()
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          snapshot_value(
              *latest),
          std::int64_t{8});
    }

    TEST(RoomSnapshotTest, AutomaticallySnapshotsAtConfiguredInterval)
    {
      Config config;

      config.snapshotEveryEvents = 2;
      config.snapshotsToKeep = 3;

      RoomFixture fixture =
          make_fixture(
              config);

      dispatch_command(
          *fixture.room,
          make_command(
              5));

      EXPECT_EQ(
          fixture.snapshotStore
              ->count(
                  make_room_id()),
          0U);

      dispatch_command(
          *fixture.room,
          make_command(
              3));

      EXPECT_EQ(
          fixture.snapshotStore
              ->count(
                  make_room_id()),
          1U);

      const auto snapshot =
          fixture.snapshotStore
              ->load_latest(
                  make_room_id());

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot->room_version()
              .value(),
          VersionValue{2});

      EXPECT_EQ(
          snapshot_value(
              *snapshot),
          std::int64_t{8});
    }

    TEST(RoomSnapshotTest, DisabledIntervalDoesNotCreateAutomaticSnapshot)
    {
      Config config;

      config.snapshotEveryEvents = 0;
      config.snapshotsToKeep = 3;

      RoomFixture fixture =
          make_fixture(
              config);

      dispatch_command(
          *fixture.room,
          make_command(
              5));

      dispatch_command(
          *fixture.room,
          make_command(
              3));

      dispatch_command(
          *fixture.room,
          make_command(
              2));

      EXPECT_EQ(
          fixture.snapshotStore
              ->count(
                  make_room_id()),
          0U);
    }

    TEST(RoomSnapshotTest, RetainsConfiguredNumberOfSnapshots)
    {
      Config config;

      config.snapshotEveryEvents = 1;
      config.snapshotsToKeep = 2;

      RoomFixture fixture =
          make_fixture(
              config);

      dispatch_command(
          *fixture.room,
          make_command(
              1));

      dispatch_command(
          *fixture.room,
          make_command(
              2));

      dispatch_command(
          *fixture.room,
          make_command(
              3));

      EXPECT_EQ(
          fixture.snapshotStore
              ->count(
                  make_room_id()),
          2U);

      const auto recent =
          fixture.snapshotStore
              ->load_recent(
                  make_room_id(),
                  10);

      ASSERT_EQ(
          recent.size(),
          2U);

      EXPECT_EQ(
          recent[0]
              .room_version()
              .value(),
          VersionValue{3});

      EXPECT_EQ(
          recent[1]
              .room_version()
              .value(),
          VersionValue{2});
    }

  } // namespace

} // namespace vix::realtime
