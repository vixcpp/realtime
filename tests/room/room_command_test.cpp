/**
 *
 * @file room_command_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for command processing in Vix Realtime rooms.
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
#include <string>
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

        ++appliedEventCount_;
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
              "unsupported counter schema"};
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

      [[nodiscard]] std::size_t
      applied_event_count() const noexcept
      {
        return appliedEventCount_;
      }

    private:
      std::int64_t value_{0};
      std::size_t appliedEventCount_{0};
    };

    class CounterHandler final : public RoomHandler
    {
    public:
      [[nodiscard]] CommandResult handle_command(
          const RoomCommand &command,
          const RoomState &state,
          const RoomContext &context) override
      {
        ++callCount_;

        lastRoomId_ =
            context.room_id();

        lastVersion_ =
            context.room_version();

        lastEventId_ =
            context.last_event_id();

        lastRequestId_ =
            command.request_id();

        lastCorrelationId_ =
            command.correlation_id();

        const auto *counterState =
            dynamic_cast<const CounterState *>(
                &state);

        if (counterState == nullptr)
        {
          return CommandResult::rejected(
              ErrorCode::CorruptedState,
              "invalid counter state");
        }

        observedValue_ =
            counterState->value();

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

        if (!payload.contains("amount") ||
            !payload.at("amount")
                 .is_number_integer())
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "amount is required");
        }

        const std::int64_t amount =
            payload.at("amount")
                .get<std::int64_t>();

        if (amount <= 0)
        {
          return CommandResult::rejected(
              ErrorCode::CommandRejected,
              "amount must be positive");
        }

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

      [[nodiscard]] std::size_t
      call_count() const noexcept
      {
        return callCount_;
      }

      [[nodiscard]] std::int64_t
      observed_value() const noexcept
      {
        return observedValue_;
      }

      [[nodiscard]] const RoomId &
      last_room_id() const noexcept
      {
        return lastRoomId_;
      }

      [[nodiscard]] const RoomVersion &
      last_version() const noexcept
      {
        return lastVersion_;
      }

      [[nodiscard]] const EventId &
      last_event_id() const noexcept
      {
        return lastEventId_;
      }

      [[nodiscard]] const RequestId &
      last_request_id() const noexcept
      {
        return lastRequestId_;
      }

      [[nodiscard]] const CorrelationId &
      last_correlation_id() const noexcept
      {
        return lastCorrelationId_;
      }

    private:
      std::size_t callCount_{0};
      std::int64_t observedValue_{0};
      RoomId lastRoomId_{};
      RoomVersion lastVersion_{};
      EventId lastEventId_{};
      RequestId lastRequestId_{};
      CorrelationId lastCorrelationId_{};
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

      CounterHandler *handler{nullptr};

      std::unique_ptr<Room> room{};
    };

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
              "counter/other"}};
    }

    [[nodiscard]] SessionId make_session_id()
    {
      return SessionId{
          std::string_view{
              "session-42"}};
    }

    [[nodiscard]] RoomCommand make_command(
        std::int64_t amount,
        std::string type =
            "counter.increment",
        RoomId roomId =
            make_room_id())
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      RoomCommand command{
          std::move(roomId),
          make_session_id(),
          std::move(type),
          std::move(payload),
          RequestId{
              "request-42"}};

      command.set_correlation_id(
          CorrelationId{
              "correlation-84"});

      return command;
    }

    [[nodiscard]] RoomFixture make_fixture(
        std::int64_t initialValue = 0)
    {
      RoomFixture fixture;

      auto handler =
          std::make_unique<
              CounterHandler>();

      fixture.handler =
          handler.get();

      fixture.room =
          std::make_unique<Room>(
              make_room_id(),
              std::make_unique<CounterState>(
                  initialValue),
              std::move(handler),
              fixture.eventStore,
              fixture.snapshotStore,
              Config{});

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

    TEST(RoomCommandTest, DispatchesCommandToHandler)
    {
      RoomFixture fixture =
          make_fixture(
              10);

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  3));

      EXPECT_TRUE(
          result.is_accepted());

      ASSERT_NE(
          fixture.handler,
          nullptr);

      EXPECT_EQ(
          fixture.handler->call_count(),
          1U);

      EXPECT_EQ(
          fixture.handler->last_room_id(),
          make_room_id());

      EXPECT_EQ(
          fixture.handler->last_request_id(),
          "request-42");

      EXPECT_EQ(
          fixture.handler->last_correlation_id(),
          "correlation-84");
    }

    TEST(RoomCommandTest, HandlerReceivesCurrentState)
    {
      RoomFixture fixture =
          make_fixture(
              10);

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  3));

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          fixture.handler
              ->observed_value(),
          std::int64_t{10});
    }

    TEST(RoomCommandTest, AppliesAcceptedCommandEvent)
    {
      RoomFixture fixture =
          make_fixture(
              10);

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  3));

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{13});

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .applied_event_count(),
          1U);
    }

    TEST(RoomCommandTest, AdvancesRoomStreamPosition)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  3));

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          fixture.room
              ->version()
              .value(),
          VersionValue{1});

      EXPECT_EQ(
          fixture.room
              ->last_event_id()
              .value(),
          EventIdValue{1});
    }

    TEST(RoomCommandTest, ReturnsPersistedEvent)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  3));

      ASSERT_TRUE(
          result.is_accepted());

      ASSERT_EQ(
          result.events().size(),
          1U);

      const RoomEvent &event =
          result.events().front();

      EXPECT_EQ(
          event.room_id(),
          make_room_id());

      EXPECT_EQ(
          event.type(),
          "counter.incremented");

      EXPECT_EQ(
          event.event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          event.room_version()
              .value(),
          VersionValue{1});
    }

    TEST(RoomCommandTest, RejectsUnsupportedCommandWithoutMutation)
    {
      RoomFixture fixture =
          make_fixture(
              10);

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  1,
                  "counter.reset"));

      EXPECT_TRUE(
          result.is_rejected());

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::InvalidCommand);

      EXPECT_TRUE(
          result.events().empty());

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{10});

      EXPECT_TRUE(
          fixture.room
              ->version()
              .is_initial());

      EXPECT_TRUE(
          fixture.room
              ->last_event_id()
              .empty());

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          0U);
    }

    TEST(RoomCommandTest, RejectsInvalidAmountWithoutMutation)
    {
      RoomFixture fixture =
          make_fixture(
              10);

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  0));

      EXPECT_TRUE(
          result.is_rejected());

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::CommandRejected);

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{10});

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          0U);
    }

    TEST(RoomCommandTest, ProcessesSequentialCommandsAgainstLatestState)
    {
      RoomFixture fixture =
          make_fixture(
              10);

      const CommandResult first =
          dispatch_command(
              *fixture.room,
              make_command(
                  3));

      const CommandResult second =
          dispatch_command(
              *fixture.room,
              make_command(
                  5));

      EXPECT_TRUE(
          first.is_accepted());

      EXPECT_TRUE(
          second.is_accepted());

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{18});

      EXPECT_EQ(
          fixture.room
              ->version()
              .value(),
          VersionValue{2});

      EXPECT_EQ(
          fixture.room
              ->last_event_id()
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          fixture.handler
              ->observed_value(),
          std::int64_t{13});
    }

    TEST(RoomCommandTest, ContextTracksLatestRoomPosition)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult first =
          dispatch_command(
              *fixture.room,
              make_command(
                  3));

      ASSERT_TRUE(
          first.is_accepted());

      const CommandResult second =
          dispatch_command(
              *fixture.room,
              make_command(
                  5));

      ASSERT_TRUE(
          second.is_accepted());

      EXPECT_EQ(
          fixture.handler
              ->last_version()
              .value(),
          VersionValue{1});

      EXPECT_EQ(
          fixture.handler
              ->last_event_id()
              .value(),
          EventIdValue{1});
    }

    TEST(RoomCommandTest, AcceptsMatchingExpectedVersion)
    {
      RoomFixture fixture =
          make_fixture();

      RoomCommand command =
          make_command(
              3);

      command.set_expected_version(
          RoomVersion{});

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              command);

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          fixture.room
              ->version()
              .value(),
          VersionValue{1});
    }

    TEST(RoomCommandTest, RejectsStaleExpectedVersion)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult first =
          dispatch_command(
              *fixture.room,
              make_command(
                  3));

      ASSERT_TRUE(
          first.is_accepted());

      RoomCommand stale =
          make_command(
              5);

      stale.set_expected_version(
          RoomVersion{});

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              stale);

      EXPECT_TRUE(
          result.is_rejected());

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{3});

      EXPECT_EQ(
          fixture.room
              ->version()
              .value(),
          VersionValue{1});

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          1U);
    }

    TEST(RoomCommandTest, RejectsCommandForDifferentRoom)
    {
      RoomFixture fixture =
          make_fixture(
              10);

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  3,
                  "counter.increment",
                  make_other_room_id()));

      EXPECT_TRUE(
          result.is_rejected());

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{10});

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          0U);
    }

  } // namespace

} // namespace vix::realtime
