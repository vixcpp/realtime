/**
 *
 * @file room_backpressure_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for command backpressure in Vix Realtime rooms.
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

#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

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

    private:
      std::int64_t value_{0};
    };

    class BlockingHandler final : public RoomHandler
    {
    public:
      [[nodiscard]] CommandResult handle_command(
          const RoomCommand &command,
          const RoomState &,
          const RoomContext &) override
      {
        const std::size_t callIndex =
            callCount_.fetch_add(
                1,
                std::memory_order_relaxed);

        if (callIndex == 0)
        {
          {
            std::lock_guard lock{
                mutex_};

            firstEntered_ = true;
          }

          enteredCondition_.notify_all();

          std::unique_lock lock{
              mutex_};

          releaseCondition_.wait(
              lock,
              [this]
              {
                return released_;
              });
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

      [[nodiscard]] bool wait_until_first_entered(
          std::chrono::milliseconds timeout)
      {
        std::unique_lock lock{
            mutex_};

        return enteredCondition_.wait_for(
            lock,
            timeout,
            [this]
            {
              return firstEntered_;
            });
      }

      void release()
      {
        {
          std::lock_guard lock{
              mutex_};

          released_ = true;
        }

        releaseCondition_.notify_all();
      }

      [[nodiscard]] std::size_t
      call_count() const noexcept
      {
        return callCount_.load(
            std::memory_order_relaxed);
      }

    private:
      std::atomic<std::size_t>
          callCount_{0};

      std::mutex mutex_{};
      std::condition_variable
          enteredCondition_{};
      std::condition_variable
          releaseCondition_{};

      bool firstEntered_{false};
      bool released_{false};
    };

    struct CommandExecution
    {
      std::optional<CommandResult> result{};
      bool threw{false};
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

      BlockingHandler *handler{nullptr};
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
              "counter/secondary"}};
    }

    [[nodiscard]] SessionId make_session_id(
        std::string_view value)
    {
      return SessionId{
          value};
    }

    [[nodiscard]] RoomCommand make_command(
        RoomId roomId,
        SessionId sessionId,
        std::int64_t amount,
        std::string requestId)
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      return RoomCommand{
          std::move(roomId),
          std::move(sessionId),
          "counter.increment",
          std::move(payload),
          std::move(requestId)};
    }

    [[nodiscard]] RoomFixture make_fixture(
        RoomId roomId = make_room_id(),
        std::size_t maximumPendingCommands = 1)
    {
      RoomFixture fixture;
      auto handler =
          std::make_unique<
              BlockingHandler>();

      fixture.handler =
          handler.get();

      Config config;

      config.maxPendingCommandsPerRoom =
          maximumPendingCommands;

      config.snapshotEveryEvents = 0;
      config.snapshotOnRoomClose = false;

      fixture.room =
          std::make_unique<Room>(
              std::move(roomId),
              std::make_unique<CounterState>(),
              std::move(handler),
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

    void execute(
        Room &room,
        RoomCommand command,
        CommandExecution &execution)
    {
      try
      {
        execution.result =
            dispatch_command(
                room,
                command);
      }
      catch (const Error &)
      {
        execution.threw = true;
      }
    }

    [[nodiscard]] std::size_t
    accepted_count(
        const std::vector<CommandExecution> &executions)
    {
      std::size_t count = 0;

      for (const CommandExecution &execution :
           executions)
      {
        if (execution.result.has_value() &&
            execution.result->is_accepted())
        {
          ++count;
        }
      }

      return count;
    }

    [[nodiscard]] std::size_t
    queue_rejected_count(
        const std::vector<CommandExecution> &executions)
    {
      std::size_t count = 0;

      for (const CommandExecution &execution :
           executions)
      {
        if (execution.threw)
        {
          ++count;
          continue;
        }

        if (execution.result.has_value() &&
            execution.result->is_rejected() &&
            execution.result->error_code() ==
                ErrorCode::CommandQueueFull)
        {
          ++count;
        }
      }

      return count;
    }

    TEST(RoomBackpressureTest, SequentialCommandsDoNotHitLimit)
    {
      RoomFixture fixture =
          make_fixture(
              make_room_id(),
              1);

      fixture.handler->release();

      for (std::int64_t value = 1;
           value <= 10;
           ++value)
      {
        const CommandResult result =
            dispatch_command(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-1"),
                    1,
                    "request-" +
                        std::to_string(
                            value)));

        EXPECT_TRUE(
            result.is_accepted());
      }

      EXPECT_EQ(
          fixture.handler->call_count(),
          10U);

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          10U);

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{10});
    }

    TEST(RoomBackpressureTest, RejectsCommandsWhenPendingQueueIsFull)
    {
      RoomFixture fixture =
          make_fixture(
              make_room_id(),
              1);

      std::vector<CommandExecution>
          executions(3);

      std::thread first{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-1"),
                    1,
                    "request-1"),
                executions[0]);
          }};

      ASSERT_TRUE(
          fixture.handler
              ->wait_until_first_entered(
                  std::chrono::seconds{
                      1}));

      std::thread second{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-2"),
                    1,
                    "request-2"),
                executions[1]);
          }};

      std::thread third{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-3"),
                    1,
                    "request-3"),
                executions[2]);
          }};

      std::this_thread::sleep_for(
          std::chrono::milliseconds{
              50});

      fixture.handler->release();

      first.join();
      second.join();
      third.join();

      EXPECT_GE(
          queue_rejected_count(
              executions),
          1U);

      EXPECT_GE(
          accepted_count(
              executions),
          1U);

      EXPECT_EQ(
          accepted_count(
              executions) +
              queue_rejected_count(
                  executions),
          3U);
    }

    TEST(RoomBackpressureTest, RejectedCommandDoesNotReachHandler)
    {
      RoomFixture fixture =
          make_fixture(
              make_room_id(),
              1);

      std::vector<CommandExecution>
          executions(3);

      std::thread first{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-1"),
                    1,
                    "request-1"),
                executions[0]);
          }};

      ASSERT_TRUE(
          fixture.handler
              ->wait_until_first_entered(
                  std::chrono::seconds{
                      1}));

      std::thread second{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-2"),
                    1,
                    "request-2"),
                executions[1]);
          }};

      std::thread third{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-3"),
                    1,
                    "request-3"),
                executions[2]);
          }};

      std::this_thread::sleep_for(
          std::chrono::milliseconds{
              50});

      fixture.handler->release();

      first.join();
      second.join();
      third.join();

      const std::size_t accepted =
          accepted_count(
              executions);

      EXPECT_EQ(
          fixture.handler->call_count(),
          accepted);

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          accepted);

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          static_cast<std::int64_t>(
              accepted));
    }

    TEST(RoomBackpressureTest, QueueFullResultContainsNoEvents)
    {
      RoomFixture fixture =
          make_fixture(
              make_room_id(),
              1);

      std::vector<CommandExecution>
          executions(3);

      std::thread first{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-1"),
                    1,
                    "request-1"),
                executions[0]);
          }};

      ASSERT_TRUE(
          fixture.handler
              ->wait_until_first_entered(
                  std::chrono::seconds{
                      1}));

      std::thread second{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-2"),
                    1,
                    "request-2"),
                executions[1]);
          }};

      std::thread third{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-3"),
                    1,
                    "request-3"),
                executions[2]);
          }};

      std::this_thread::sleep_for(
          std::chrono::milliseconds{
              50});

      fixture.handler->release();

      first.join();
      second.join();
      third.join();

      bool foundRejectedResult = false;

      for (const CommandExecution &execution :
           executions)
      {
        if (!execution.result.has_value() ||
            !execution.result->is_rejected())
        {
          continue;
        }

        if (execution.result->error_code() !=
            ErrorCode::CommandQueueFull)
        {
          continue;
        }

        foundRejectedResult = true;

        EXPECT_TRUE(
            execution.result
                ->events()
                .empty());

        EXPECT_NO_THROW(
            execution.result
                ->validate());
      }

      EXPECT_TRUE(
          foundRejectedResult ||
          queue_rejected_count(
              executions) > 0U);
    }

    TEST(RoomBackpressureTest, QueueAcceptsNewCommandAfterPressureDrops)
    {
      RoomFixture fixture =
          make_fixture(
              make_room_id(),
              1);

      std::vector<CommandExecution>
          executions(3);

      std::thread first{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-1"),
                    1,
                    "request-1"),
                executions[0]);
          }};

      ASSERT_TRUE(
          fixture.handler
              ->wait_until_first_entered(
                  std::chrono::seconds{
                      1}));

      std::thread second{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-2"),
                    1,
                    "request-2"),
                executions[1]);
          }};

      std::thread third{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-3"),
                    1,
                    "request-3"),
                executions[2]);
          }};

      std::this_thread::sleep_for(
          std::chrono::milliseconds{
              50});

      fixture.handler->release();

      first.join();
      second.join();
      third.join();

      const std::size_t acceptedBefore =
          accepted_count(
              executions);

      const CommandResult next =
          dispatch_command(
              *fixture.room,
              make_command(
                  make_room_id(),
                  make_session_id(
                      "session-4"),
                  1,
                  "request-4"));

      EXPECT_TRUE(
          next.is_accepted());

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          acceptedBefore + 1U);

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          static_cast<std::int64_t>(
              acceptedBefore + 1U));
    }

    TEST(RoomBackpressureTest, PressureDoesNotCloseRoom)
    {
      RoomFixture fixture =
          make_fixture(
              make_room_id(),
              1);

      std::vector<CommandExecution>
          executions(3);

      std::thread first{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-1"),
                    1,
                    "request-1"),
                executions[0]);
          }};

      ASSERT_TRUE(
          fixture.handler
              ->wait_until_first_entered(
                  std::chrono::seconds{
                      1}));

      std::thread second{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-2"),
                    1,
                    "request-2"),
                executions[1]);
          }};

      std::thread third{
          [&]
          {
            execute(
                *fixture.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-3"),
                    1,
                    "request-3"),
                executions[2]);
          }};

      std::this_thread::sleep_for(
          std::chrono::milliseconds{
              50});

      fixture.handler->release();

      first.join();
      second.join();
      third.join();

      EXPECT_TRUE(
          fixture.room->is_open());

      EXPECT_FALSE(
          fixture.room->is_closed());

      EXPECT_EQ(
          fixture.room->status(),
          RoomStatus::Open);
    }

    TEST(RoomBackpressureTest, BackpressureIsIsolatedPerRoom)
    {
      RoomFixture firstRoom =
          make_fixture(
              make_room_id(),
              1);

      RoomFixture secondRoom =
          make_fixture(
              make_other_room_id(),
              1);

      secondRoom.handler->release();

      CommandExecution blockedExecution;

      std::thread blocked{
          [&]
          {
            execute(
                *firstRoom.room,
                make_command(
                    make_room_id(),
                    make_session_id(
                        "session-1"),
                    1,
                    "request-1"),
                blockedExecution);
          }};

      ASSERT_TRUE(
          firstRoom.handler
              ->wait_until_first_entered(
                  std::chrono::seconds{
                      1}));

      const CommandResult secondRoomResult =
          dispatch_command(
              *secondRoom.room,
              make_command(
                  make_other_room_id(),
                  make_session_id(
                      "session-2"),
                  1,
                  "request-2"));

      EXPECT_TRUE(
          secondRoomResult.is_accepted());

      EXPECT_EQ(
          secondRoom.eventStore
              ->count(
                  make_other_room_id()),
          1U);

      EXPECT_EQ(
          counter_state(
              *secondRoom.room)
              .value(),
          std::int64_t{1});

      firstRoom.handler->release();

      blocked.join();

      ASSERT_TRUE(
          blockedExecution.result
              .has_value());

      EXPECT_TRUE(
          blockedExecution.result
              ->is_accepted());
    }

  } // namespace

} // namespace vix::realtime
