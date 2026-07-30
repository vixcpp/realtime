/**
 *
 * @file snapshot_and_replay_test.cpp
 * @author Gaspard Kirira
 * @brief Integration tests for snapshots and event replay in Vix Realtime.
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
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    void close_room(
        RoomType &room)
    {
      if constexpr (
          requires {
            room.close();
          })
      {
        room.close();
      }
      else if constexpr (
          requires {
            room.shutdown();
          })
      {
        room.shutdown();
      }
      else if constexpr (
          requires {
            room.stop();
          })
      {
        room.stop();
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room close API");
      }
    }

    class CounterState final : public RoomState
    {
    public:
      explicit CounterState(
          std::int64_t value = 0)
          : value_(
                value)
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
          throw Error{
              ErrorCode::EventApplyFailure,
              "unsupported counter event"};
        }

        const auto payload =
            vix::json::to_json(
                event.payload());

        value_ +=
            payload.at("amount")
                .get<std::int64_t>();

        if (!event.event_id()
                 .empty())
        {
          appliedEventIds_.push_back(
              event.event_id());
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

        appliedEventIds_.clear();
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

      [[nodiscard]] const std::vector<EventId> &
      applied_event_ids() const noexcept
      {
        return appliedEventIds_;
      }

    private:
      std::int64_t value_{0};

      std::vector<EventId>
          appliedEventIds_{};
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
              "unsupported counter command");
        }

        const auto payload =
            vix::json::to_json(
                command.payload());

        if (!payload.contains("amount") ||
            !payload.at("amount")
                 .is_number_integer())
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "counter amount is required");
        }

        JsonObject eventPayload;

        eventPayload.set_i64(
            "amount",
            payload.at("amount")
                .get<std::int64_t>());

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

    struct PersistenceFixture
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

    [[nodiscard]] RoomId make_room_id(
        std::string_view value =
            "counter/main")
    {
      return RoomId{
          value};
    }

    [[nodiscard]] SessionId make_session_id()
    {
      return SessionId{
          std::string_view{
              "session-42"}};
    }

    [[nodiscard]] RoomCommand make_command(
        std::int64_t amount,
        std::size_t index)
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
          "request-" +
              std::to_string(
                  index)};

      command.set_correlation_id(
          CorrelationId{
              "correlation-42"});

      return command;
    }

    [[nodiscard]] Config make_config(
        std::size_t snapshotEveryEvents)
    {
      Config config;

      config.snapshotEveryEvents =
          snapshotEveryEvents;

      config.snapshotOnRoomClose =
          false;

      config.snapshotsToKeep =
          5;

      config.restoreRoomsOnOpen =
          true;

      return config;
    }

    [[nodiscard]] std::unique_ptr<Room>
    make_room(
        const PersistenceFixture &fixture,
        const Config &config)
    {
      return std::make_unique<Room>(
          make_room_id(),
          std::make_unique<CounterState>(),
          std::make_unique<CounterHandler>(),
          fixture.eventStore,
          fixture.snapshotStore,
          config);
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

    void append_commands(
        Room &room,
        std::size_t count,
        std::int64_t amount = 1,
        std::size_t startingIndex = 1)
    {
      for (std::size_t offset = 0;
           offset < count;
           ++offset)
      {
        const std::size_t index =
            startingIndex +
            offset;

        const CommandResult result =
            dispatch_command(
                room,
                make_command(
                    amount,
                    index));

        ASSERT_TRUE(
            result.is_accepted());
      }
    }

    TEST(
        SnapshotAndReplayTest,
        RestoresSnapshotAndReplaysFollowingEvents)
    {
      PersistenceFixture fixture;

      const Config config =
          make_config(
              3);

      auto first =
          make_room(
              fixture,
              config);

      first->open();

      append_commands(
          *first,
          5);

      ASSERT_EQ(
          counter_state(
              *first)
              .value(),
          std::int64_t{5});

      const auto snapshot =
          fixture.snapshotStore
              ->load_latest(
                  make_room_id());

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot->room_version()
              .value(),
          VersionValue{3});

      EXPECT_EQ(
          snapshot->last_event_id()
              .value(),
          EventIdValue{3});

      close_room(
          *first);

      auto restored =
          make_room(
              fixture,
              config);

      restored->open();

      EXPECT_TRUE(
          restored->is_open());

      EXPECT_EQ(
          counter_state(
              *restored)
              .value(),
          std::int64_t{5});

      EXPECT_EQ(
          restored->version()
              .value(),
          VersionValue{5});

      EXPECT_EQ(
          restored->last_event_id()
              .value(),
          EventIdValue{5});

      const auto &replayed =
          counter_state(
              *restored)
              .applied_event_ids();

      ASSERT_EQ(
          replayed.size(),
          2U);

      EXPECT_EQ(
          replayed[0]
              .value(),
          EventIdValue{4});

      EXPECT_EQ(
          replayed[1]
              .value(),
          EventIdValue{5});
    }

    TEST(
        SnapshotAndReplayTest,
        ReopeningDoesNotDuplicatePersistedEvents)
    {
      PersistenceFixture fixture;

      const Config config =
          make_config(
              3);

      auto first =
          make_room(
              fixture,
              config);

      first->open();

      append_commands(
          *first,
          5);

      ASSERT_EQ(
          fixture.eventStore
              ->count(
                  make_room_id()),
          5U);

      close_room(
          *first);

      auto restored =
          make_room(
              fixture,
              config);

      restored->open();

      EXPECT_EQ(
          fixture.eventStore
              ->count(
                  make_room_id()),
          5U);

      EXPECT_EQ(
          fixture.eventStore
              ->latest_event_id(
                  make_room_id())
              .value(),
          EventIdValue{5});

      EXPECT_EQ(
          counter_state(
              *restored)
              .value(),
          std::int64_t{5});
    }

    TEST(
        SnapshotAndReplayTest,
        RecoveredRoomContinuesFromLatestPosition)
    {
      PersistenceFixture fixture;

      const Config config =
          make_config(
              3);

      auto first =
          make_room(
              fixture,
              config);

      first->open();

      append_commands(
          *first,
          5);

      close_room(
          *first);

      auto restored =
          make_room(
              fixture,
              config);

      restored->open();

      const CommandResult result =
          dispatch_command(
              *restored,
              make_command(
                  2,
                  6));

      ASSERT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          counter_state(
              *restored)
              .value(),
          std::int64_t{7});

      EXPECT_EQ(
          restored->version()
              .value(),
          VersionValue{6});

      EXPECT_EQ(
          restored->last_event_id()
              .value(),
          EventIdValue{6});

      EXPECT_EQ(
          fixture.eventStore
              ->count(
                  make_room_id()),
          6U);

      const auto events =
          fixture.eventStore
              ->load_after(
                  make_room_id(),
                  EventId{
                      EventIdValue{5}},
                  10);

      ASSERT_EQ(
          events.size(),
          1U);

      EXPECT_EQ(
          events.front()
              .event_id()
              .value(),
          EventIdValue{6});

      EXPECT_EQ(
          events.front()
              .room_version()
              .value(),
          VersionValue{6});
    }

    TEST(
        SnapshotAndReplayTest,
        RestoresEntireHistoryWhenNoSnapshotExists)
    {
      PersistenceFixture fixture;

      const Config config =
          make_config(
              0);

      auto first =
          make_room(
              fixture,
              config);

      first->open();

      append_commands(
          *first,
          4);

      ASSERT_FALSE(
          fixture.snapshotStore
              ->load_latest(
                  make_room_id())
              .has_value());

      close_room(
          *first);

      auto restored =
          make_room(
              fixture,
              config);

      restored->open();

      EXPECT_EQ(
          counter_state(
              *restored)
              .value(),
          std::int64_t{4});

      EXPECT_EQ(
          restored->version()
              .value(),
          VersionValue{4});

      EXPECT_EQ(
          restored->last_event_id()
              .value(),
          EventIdValue{4});

      const auto &replayed =
          counter_state(
              *restored)
              .applied_event_ids();

      ASSERT_EQ(
          replayed.size(),
          4U);

      EXPECT_EQ(
          replayed[0]
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          replayed[1]
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          replayed[2]
              .value(),
          EventIdValue{3});

      EXPECT_EQ(
          replayed[3]
              .value(),
          EventIdValue{4});
    }

    TEST(
        SnapshotAndReplayTest,
        UsesLatestAvailableSnapshot)
    {
      PersistenceFixture fixture;

      const Config config =
          make_config(
              2);

      auto first =
          make_room(
              fixture,
              config);

      first->open();

      append_commands(
          *first,
          5);

      ASSERT_EQ(
          fixture.snapshotStore
              ->count(
                  make_room_id()),
          2U);

      const auto snapshot =
          fixture.snapshotStore
              ->load_latest(
                  make_room_id());

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot->room_version()
              .value(),
          VersionValue{4});

      EXPECT_EQ(
          snapshot->last_event_id()
              .value(),
          EventIdValue{4});

      const auto snapshotState =
          vix::json::to_json(
              snapshot->state());

      EXPECT_EQ(
          snapshotState.at("value")
              .get<std::int64_t>(),
          std::int64_t{4});

      close_room(
          *first);

      auto restored =
          make_room(
              fixture,
              config);

      restored->open();

      EXPECT_EQ(
          counter_state(
              *restored)
              .value(),
          std::int64_t{5});

      const auto &replayed =
          counter_state(
              *restored)
              .applied_event_ids();

      ASSERT_EQ(
          replayed.size(),
          1U);

      EXPECT_EQ(
          replayed.front()
              .value(),
          EventIdValue{5});
    }

    TEST(
        SnapshotAndReplayTest,
        SnapshotStateIsNotMutatedByLaterEvents)
    {
      PersistenceFixture fixture;

      const Config config =
          make_config(
              3);

      auto room =
          make_room(
              fixture,
              config);

      room->open();

      append_commands(
          *room,
          5);

      const auto snapshot =
          fixture.snapshotStore
              ->load_latest(
                  make_room_id());

      ASSERT_TRUE(
          snapshot.has_value());

      const auto snapshotState =
          vix::json::to_json(
              snapshot->state());

      EXPECT_EQ(
          snapshotState.at("value")
              .get<std::int64_t>(),
          std::int64_t{3});

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{5});
    }

    TEST(
        SnapshotAndReplayTest,
        MultipleRestorationsRemainDeterministic)
    {
      PersistenceFixture fixture;

      const Config config =
          make_config(
              3);

      auto original =
          make_room(
              fixture,
              config);

      original->open();

      append_commands(
          *original,
          5);

      close_room(
          *original);

      auto firstRestoration =
          make_room(
              fixture,
              config);

      auto secondRestoration =
          make_room(
              fixture,
              config);

      firstRestoration->open();
      secondRestoration->open();

      EXPECT_EQ(
          counter_state(
              *firstRestoration)
              .value(),
          std::int64_t{5});

      EXPECT_EQ(
          counter_state(
              *secondRestoration)
              .value(),
          std::int64_t{5});

      EXPECT_EQ(
          firstRestoration->version(),
          secondRestoration->version());

      EXPECT_EQ(
          firstRestoration
              ->last_event_id(),
          secondRestoration
              ->last_event_id());

      EXPECT_EQ(
          fixture.eventStore
              ->count(
                  make_room_id()),
          5U);
    }

  } // namespace

} // namespace vix::realtime
