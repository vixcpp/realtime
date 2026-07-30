/**
 *
 * @file room_lifecycle_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the lifecycle of Vix Realtime rooms.
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
#include <optional>
#include <string_view>
#include <utility>

#include <vix/json/json.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/session.hpp>
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

    template <typename RoomType>
    [[nodiscard]] bool join_session(
        RoomType &room,
        const std::shared_ptr<Session> &session)
    {
      if constexpr (
          requires {
            {
              room.join(session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.join(
            session);
      }
      else if constexpr (
          requires {
            room.join(session);
          })
      {
        room.join(
            session);

        return true;
      }
      else if constexpr (
          requires {
            {
              room.join(*session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.join(
            *session);
      }
      else if constexpr (
          requires {
            room.join(*session);
          })
      {
        room.join(
            *session);

        return true;
      }
      else if constexpr (
          requires {
            {
              room.join_session(session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.join_session(
            session);
      }
      else if constexpr (
          requires {
            {
              room.add_session(session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.add_session(
            session);
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room session join API");
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
        std::int64_t amount = 1)
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
        Config config = {})
    {
      RoomFixture fixture;

      fixture.room =
          std::make_unique<Room>(
              make_room_id(),
              std::make_unique<CounterState>(),
              std::make_unique<CounterHandler>(),
              fixture.eventStore,
              fixture.snapshotStore,
              std::move(config));

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

    [[nodiscard]] bool command_is_rejected(
        Room &room,
        const RoomCommand &command)
    {
      try
      {
        return dispatch_command(
                   room,
                   command)
            .is_rejected();
      }
      catch (const Error &)
      {
        return true;
      }
    }

    [[nodiscard]] bool join_is_rejected(
        Room &room,
        const std::shared_ptr<Session> &session)
    {
      try
      {
        return !join_session(
            room,
            session);
      }
      catch (const Error &)
      {
        return true;
      }
    }

    TEST(RoomLifecycleTest, StartsInCreatedState)
    {
      const RoomFixture fixture =
          make_fixture();

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Created);

      EXPECT_FALSE(
          fixture.room->is_open());

      EXPECT_FALSE(
          fixture.room->is_closed());
    }

    TEST(RoomLifecycleTest, OpensCreatedRoom)
    {
      RoomFixture fixture =
          make_fixture();

      fixture.room->open();

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Open);

      EXPECT_TRUE(
          fixture.room->is_open());

      EXPECT_FALSE(
          fixture.room->is_closed());
    }

    TEST(RoomLifecycleTest, OpeningOpenRoomIsIdempotent)
    {
      RoomFixture fixture =
          make_fixture();

      fixture.room->open();

      EXPECT_NO_THROW(
          fixture.room->open());

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Open);

      EXPECT_TRUE(
          fixture.room->is_open());
    }

    TEST(RoomLifecycleTest, ProcessesCommandsWhileOpen)
    {
      RoomFixture fixture =
          make_fixture();

      fixture.room->open();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  5));

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{5});

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          1U);
    }

    TEST(RoomLifecycleTest, ClosesOpenRoom)
    {
      RoomFixture fixture =
          make_fixture();

      fixture.room->open();

      close_room(
          *fixture.room);

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Closed);

      EXPECT_FALSE(
          fixture.room->is_open());

      EXPECT_TRUE(
          fixture.room->is_closed());
    }

    TEST(RoomLifecycleTest, ClosingRoomTwiceIsIdempotent)
    {
      RoomFixture fixture =
          make_fixture();

      fixture.room->open();

      close_room(
          *fixture.room);

      EXPECT_NO_THROW(
          close_room(
              *fixture.room));

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Closed);

      EXPECT_TRUE(
          fixture.room->is_closed());
    }

    TEST(RoomLifecycleTest, CloseRemovesAllMembers)
    {
      RoomFixture fixture =
          make_fixture();

      fixture.room->open();

      const auto first =
          std::make_shared<Session>(
              SessionId{
                  std::string_view{
                      "session-1"}});

      const auto second =
          std::make_shared<Session>(
              SessionId{
                  std::string_view{
                      "session-2"}});

      ASSERT_TRUE(
          join_session(
              *fixture.room,
              first));

      ASSERT_TRUE(
          join_session(
              *fixture.room,
              second));

      ASSERT_EQ(
          fixture.room->member_count(),
          2U);

      close_room(
          *fixture.room);

      EXPECT_EQ(
          fixture.room->member_count(),
          0U);

      EXPECT_TRUE(
          fixture.room->empty());

      EXPECT_FALSE(
          first->has_room(
              make_room_id()));

      EXPECT_FALSE(
          second->has_room(
              make_room_id()));
    }

    TEST(RoomLifecycleTest, ClosedRoomRejectsCommands)
    {
      RoomFixture fixture =
          make_fixture();

      fixture.room->open();

      close_room(
          *fixture.room);

      EXPECT_TRUE(
          command_is_rejected(
              *fixture.room,
              make_command(
                  5)));

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{0});

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          0U);
    }

    TEST(RoomLifecycleTest, ClosedRoomRejectsNewMembers)
    {
      RoomFixture fixture =
          make_fixture();

      fixture.room->open();

      close_room(
          *fixture.room);

      const auto session =
          std::make_shared<Session>(
              make_session_id());

      EXPECT_TRUE(
          join_is_rejected(
              *fixture.room,
              session));

      EXPECT_EQ(
          fixture.room->member_count(),
          0U);

      EXPECT_FALSE(
          session->has_room(
              make_room_id()));
    }

    TEST(RoomLifecycleTest, ClosedRoomCannotReturnToOpenState)
    {
      RoomFixture fixture =
          make_fixture();

      fixture.room->open();

      close_room(
          *fixture.room);

      try
      {
        fixture.room->open();
      }
      catch (const Error &)
      {
      }

      EXPECT_TRUE(
          fixture.room->is_closed());

      EXPECT_FALSE(
          fixture.room->is_open());

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Closed);
    }

    TEST(RoomLifecycleTest, ClosePreservesFinalStreamPosition)
    {
      RoomFixture fixture =
          make_fixture();

      fixture.room->open();

      ASSERT_TRUE(
          dispatch_command(
              *fixture.room,
              make_command(
                  5))
              .is_accepted());

      ASSERT_TRUE(
          dispatch_command(
              *fixture.room,
              make_command(
                  3))
              .is_accepted());

      const RoomVersion finalVersion =
          fixture.room->version();

      const EventId finalEventId =
          fixture.room->last_event_id();

      close_room(
          *fixture.room);

      EXPECT_EQ(
          fixture.room->version(),
          finalVersion);

      EXPECT_EQ(
          fixture.room->last_event_id(),
          finalEventId);

      EXPECT_EQ(
          finalVersion.value(),
          VersionValue{2});

      EXPECT_EQ(
          finalEventId.value(),
          EventIdValue{2});
    }

    TEST(RoomLifecycleTest, CloseCreatesSnapshotWhenConfigured)
    {
      Config config;

      config.snapshotEveryEvents = 0;
      config.snapshotOnRoomClose = true;
      config.snapshotsToKeep = 3;

      RoomFixture fixture =
          make_fixture(
              config);

      fixture.room->open();

      ASSERT_TRUE(
          dispatch_command(
              *fixture.room,
              make_command(
                  5))
              .is_accepted());

      close_room(
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
          VersionValue{1});

      EXPECT_EQ(
          snapshot->last_event_id()
              .value(),
          EventIdValue{1});

      const auto state =
          vix::json::to_json(
              snapshot->state());

      EXPECT_EQ(
          state.at("value")
              .get<std::int64_t>(),
          std::int64_t{5});
    }

    TEST(RoomLifecycleTest, CloseDoesNotSnapshotWhenDisabled)
    {
      Config config;

      config.snapshotEveryEvents = 0;
      config.snapshotOnRoomClose = false;

      RoomFixture fixture =
          make_fixture(
              config);

      fixture.room->open();

      ASSERT_TRUE(
          dispatch_command(
              *fixture.room,
              make_command(
                  5))
              .is_accepted());

      close_room(
          *fixture.room);

      EXPECT_FALSE(
          fixture.snapshotStore
              ->load_latest(
                  make_room_id())
              .has_value());
    }

  } // namespace

} // namespace vix::realtime
