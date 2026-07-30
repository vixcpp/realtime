/**
 *
 * @file room_failure_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for failure handling in Vix Realtime rooms.
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

    class FailingState final : public RoomState
    {
    public:
      explicit FailingState(
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

          ++successfulApplyCount_;
          return;
        }

        if (event.type() ==
            "counter.failed")
        {
          throw Error{
              ErrorCode::EventApplyFailure,
              "counter event application failed"};
        }

        throw Error{
            ErrorCode::EventApplyFailure,
            "unsupported counter event"};
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

        successfulApplyCount_ = 0;
      }

      [[nodiscard]] std::unique_ptr<RoomState>
      clone() const override
      {
        return std::make_unique<FailingState>(
            *this);
      }

      [[nodiscard]] std::int64_t
      value() const noexcept
      {
        return value_;
      }

      [[nodiscard]] std::size_t
      successful_apply_count() const noexcept
      {
        return successfulApplyCount_;
      }

    private:
      std::int64_t value_{0};
      std::size_t successfulApplyCount_{0};
    };

    class FailingHandler final : public RoomHandler
    {
    public:
      [[nodiscard]] CommandResult handle_command(
          const RoomCommand &command,
          const RoomState &,
          const RoomContext &) override
      {
        ++callCount_;

        if (command.type() ==
            "counter.reject")
        {
          return CommandResult::rejected(
              ErrorCode::CommandRejected,
              "counter command rejected");
        }

        if (command.type() ==
            "counter.throw")
        {
          throw Error{
              ErrorCode::InternalError,
              "counter handler failed"};
        }

        if (command.type() ==
            "counter.fail_apply")
        {
          return CommandResult::accepted(
              {make_event(
                  command,
                  "counter.failed",
                  1)});
        }

        if (command.type() ==
            "counter.fail_batch")
        {
          std::vector<RoomEvent> events;

          events.push_back(
              make_event(
                  command,
                  "counter.incremented",
                  10));

          events.push_back(
              make_event(
                  command,
                  "counter.failed",
                  1));

          return CommandResult::accepted(
              std::move(events));
        }

        if (command.type() ==
            "counter.increment")
        {
          return CommandResult::accepted(
              {make_event(
                  command,
                  "counter.incremented",
                  amount_from(
                      command))});
        }

        return CommandResult::rejected(
            ErrorCode::InvalidCommand,
            "unsupported command");
      }

      [[nodiscard]] std::size_t
      call_count() const noexcept
      {
        return callCount_;
      }

    private:
      [[nodiscard]] static std::int64_t amount_from(
          const RoomCommand &command)
      {
        const auto payload =
            vix::json::to_json(
                command.payload());

        return payload.at("amount")
            .get<std::int64_t>();
      }

      [[nodiscard]] static RoomEvent make_event(
          const RoomCommand &command,
          std::string type,
          std::int64_t amount)
      {
        JsonObject payload;

        payload.set_i64(
            "amount",
            amount);

        RoomEvent event{
            command.room_id(),
            std::move(type),
            std::move(payload),
            EventAudience::Room};

        event
            .set_source_session(
                command.session_id())
            .set_request_id(
                command.request_id())
            .set_correlation_id(
                command.correlation_id());

        return event;
      }

      std::size_t callCount_{0};
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

      FailingHandler *handler{nullptr};
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
        std::string type,
        std::int64_t amount = 1)
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      RoomCommand command{
          make_room_id(),
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
              FailingHandler>();

      fixture.handler =
          handler.get();

      Config config;

      config.snapshotEveryEvents = 0;
      config.snapshotOnRoomClose = false;

      fixture.room =
          std::make_unique<Room>(
              make_room_id(),
              std::make_unique<FailingState>(
                  initialValue),
              std::move(handler),
              fixture.eventStore,
              fixture.snapshotStore,
              std::move(config));

      fixture.room->open();

      return fixture;
    }

    [[nodiscard]] const FailingState &
    failing_state(
        const Room &room)
    {
      const auto *state =
          dynamic_cast<const FailingState *>(
              &room.state());

      EXPECT_NE(
          state,
          nullptr);

      return *state;
    }

    [[nodiscard]] bool command_fails(
        Room &room,
        const RoomCommand &command)
    {
      try
      {
        const CommandResult result =
            dispatch_command(
                room,
                command);

        return result.is_rejected();
      }
      catch (const Error &)
      {
        return true;
      }
    }

    TEST(RoomFailureTest, BusinessRejectionDoesNotFailRoom)
    {
      RoomFixture fixture =
          make_fixture(
              10);

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.reject"));

      EXPECT_TRUE(
          result.is_rejected());

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::CommandRejected);

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Open);

      EXPECT_TRUE(
          fixture.room->is_open());

      EXPECT_FALSE(
          fixture.room->is_closed());

      EXPECT_EQ(
          failing_state(
              *fixture.room)
              .value(),
          std::int64_t{10});

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          0U);
    }

    TEST(RoomFailureTest, HandlerExceptionMarksRoomFailed)
    {
      RoomFixture fixture =
          make_fixture();

      EXPECT_TRUE(
          command_fails(
              *fixture.room,
              make_command(
                  "counter.throw")));

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Failed);

      EXPECT_FALSE(
          fixture.room->is_open());

      EXPECT_FALSE(
          fixture.room->is_closed());

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          0U);
    }

    TEST(RoomFailureTest, FailedHandlerDoesNotMutateState)
    {
      RoomFixture fixture =
          make_fixture(
              42);

      EXPECT_TRUE(
          command_fails(
              *fixture.room,
              make_command(
                  "counter.throw")));

      EXPECT_EQ(
          failing_state(
              *fixture.room)
              .value(),
          std::int64_t{42});

      EXPECT_TRUE(
          fixture.room->version()
              .is_initial());

      EXPECT_TRUE(
          fixture.room->last_event_id()
              .empty());
    }

    TEST(RoomFailureTest, FailedRoomRejectsSubsequentCommands)
    {
      RoomFixture fixture =
          make_fixture();

      EXPECT_TRUE(
          command_fails(
              *fixture.room,
              make_command(
                  "counter.throw")));

      ASSERT_EQ(
          fixture.room->status(),
          RoomStatus::Failed);

      EXPECT_TRUE(
          command_fails(
              *fixture.room,
              make_command(
                  "counter.increment",
                  5)));

      EXPECT_EQ(
          failing_state(
              *fixture.room)
              .value(),
          std::int64_t{0});

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          0U);
    }

    TEST(RoomFailureTest, EventApplicationFailureMarksRoomFailed)
    {
      RoomFixture fixture =
          make_fixture();

      EXPECT_TRUE(
          command_fails(
              *fixture.room,
              make_command(
                  "counter.fail_apply")));

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Failed);

      EXPECT_FALSE(
          fixture.room->is_open());
    }

    TEST(RoomFailureTest, FailedEventDoesNotMutateState)
    {
      RoomFixture fixture =
          make_fixture(
              42);

      EXPECT_TRUE(
          command_fails(
              *fixture.room,
              make_command(
                  "counter.fail_apply")));

      EXPECT_EQ(
          failing_state(
              *fixture.room)
              .value(),
          std::int64_t{42});

      EXPECT_EQ(
          failing_state(
              *fixture.room)
              .successful_apply_count(),
          0U);
    }

    TEST(RoomFailureTest, FailedEventRemainsPersistedForDiagnosis)
    {
      RoomFixture fixture =
          make_fixture();

      EXPECT_TRUE(
          command_fails(
              *fixture.room,
              make_command(
                  "counter.fail_apply")));

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          1U);

      const auto events =
          fixture.eventStore->load_after(
              make_room_id(),
              EventId{},
              100);

      ASSERT_EQ(
          events.size(),
          1U);

      EXPECT_EQ(
          events.front().type(),
          "counter.failed");

      EXPECT_EQ(
          events.front()
              .event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          events.front()
              .room_version()
              .value(),
          VersionValue{1});
    }

    TEST(RoomFailureTest, BatchFailureRollsBackEntireInMemoryState)
    {
      RoomFixture fixture =
          make_fixture(
              5);

      EXPECT_TRUE(
          command_fails(
              *fixture.room,
              make_command(
                  "counter.fail_batch")));

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Failed);

      EXPECT_EQ(
          failing_state(
              *fixture.room)
              .value(),
          std::int64_t{5});

      EXPECT_EQ(
          failing_state(
              *fixture.room)
              .successful_apply_count(),
          0U);
    }

    TEST(RoomFailureTest, FailedBatchRemainsAtomicallyPersisted)
    {
      RoomFixture fixture =
          make_fixture();

      EXPECT_TRUE(
          command_fails(
              *fixture.room,
              make_command(
                  "counter.fail_batch")));

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          2U);

      const auto events =
          fixture.eventStore->load_after(
              make_room_id(),
              EventId{},
              100);

      ASSERT_EQ(
          events.size(),
          2U);

      EXPECT_EQ(
          events[0].type(),
          "counter.incremented");

      EXPECT_EQ(
          events[1].type(),
          "counter.failed");

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
          events[0]
              .room_version()
              .value(),
          VersionValue{1});

      EXPECT_EQ(
          events[1]
              .room_version()
              .value(),
          VersionValue{2});
    }

    TEST(RoomFailureTest, SuccessfulCommandBeforeFailureRemainsApplied)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult successful =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.increment",
                  5));

      ASSERT_TRUE(
          successful.is_accepted());

      ASSERT_EQ(
          failing_state(
              *fixture.room)
              .value(),
          std::int64_t{5});

      EXPECT_TRUE(
          command_fails(
              *fixture.room,
              make_command(
                  "counter.fail_apply")));

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Failed);

      EXPECT_EQ(
          failing_state(
              *fixture.room)
              .value(),
          std::int64_t{5});

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          2U);
    }

    TEST(RoomFailureTest, BusinessRejectionAllowsLaterSuccessfulCommand)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult rejected =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.reject"));

      ASSERT_TRUE(
          rejected.is_rejected());

      const CommandResult accepted =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.increment",
                  5));

      EXPECT_TRUE(
          accepted.is_accepted());

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Open);

      EXPECT_EQ(
          failing_state(
              *fixture.room)
              .value(),
          std::int64_t{5});

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          1U);
    }

  } // namespace

} // namespace vix::realtime
