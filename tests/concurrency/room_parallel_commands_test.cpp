/** @file room_parallel_commands_test.cpp */

#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <vix/json/json.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_state.hpp>

namespace vix::realtime
{
  namespace
  {
    class CounterState final : public RoomState
    {
    public:
      [[nodiscard]] SchemaVersion schema_version() const noexcept override { return SchemaVersion{1}; }
      void apply(const RoomEvent &event) override { value_ += vix::json::to_json(event.payload()).at("amount").get<std::int64_t>(); }
      [[nodiscard]] JsonObject serialize() const override { JsonObject state; state.set_i64("value", value_); return state; }
      void restore(const JsonObject &state, SchemaVersion) override { value_ = vix::json::to_json(state).at("value").get<std::int64_t>(); }
      [[nodiscard]] std::unique_ptr<RoomState> clone() const override { return std::make_unique<CounterState>(*this); }
      [[nodiscard]] std::int64_t value() const noexcept { return value_; }
    private:
      std::int64_t value_{0};
    };

    class CounterHandler final : public RoomHandler
    {
    public:
      [[nodiscard]] CommandResult handle_command(const RoomCommand &command, const RoomState &, const RoomContext &) override
      {
        RoomEvent event{command.room_id(), "counter.incremented", command.payload(), EventAudience::Room};
        return CommandResult::accepted({std::move(event)});
      }
    };

    [[nodiscard]] RoomCommand command(const RoomId &roomId, std::string requestId)
    {
      JsonObject payload;
      payload.set_i64("amount", 1);
      return RoomCommand{roomId, SessionId{std::string_view{"concurrent-session"}}, "counter.increment", std::move(payload), std::move(requestId)};
    }

    [[nodiscard]] std::shared_ptr<Room> make_room(const RoomId &roomId, Config config = {})
    {
      auto room = std::make_shared<Room>(roomId, std::make_unique<CounterState>(), std::make_unique<CounterHandler>(), std::make_shared<MemoryEventStore>(), std::make_shared<MemorySnapshotStore>(), config);
      EXPECT_TRUE(room->open().is_accepted());
      return room;
    }
  }

  TEST(RoomParallelCommandsTest, ConcurrentCommandsCommitOneContiguousStream)
  {
    Config config;
    config.maxPendingCommandsPerRoom = 64;
    const RoomId roomId{"concurrency/one-room"};
    const auto room = make_room(roomId, config);
    constexpr std::size_t commandCount = 24;
    std::barrier start{static_cast<std::ptrdiff_t>(commandCount + 1)};
    std::atomic<std::size_t> accepted{0};
    std::vector<std::thread> workers;
    workers.reserve(commandCount);
    for (std::size_t index = 0; index < commandCount; ++index)
    {
      workers.emplace_back([&, index] { start.arrive_and_wait(); if (room->execute(command(roomId, "request-" + std::to_string(index))).is_accepted()) { ++accepted; } });
    }
    start.arrive_and_wait();
    for (auto &worker : workers) { worker.join(); }

    ASSERT_EQ(accepted.load(), commandCount);
    EXPECT_EQ(room->version().value(), VersionValue{commandCount});
    EXPECT_EQ(room->last_event_id().value(), EventIdValue{commandCount});
    EXPECT_EQ(dynamic_cast<const CounterState &>(room->state()).value(), static_cast<std::int64_t>(commandCount));
  }

  TEST(RoomParallelCommandsTest, QueueSaturationDoesNotLoseAcceptedCommands)
  {
    Config config;
    config.maxPendingCommandsPerRoom = 2;
    const RoomId roomId{"concurrency/queue"};
    const auto room = make_room(roomId, config);
    EXPECT_EQ(room->enqueue(command(roomId, "one")), CommandQueueStatus::Success);
    EXPECT_EQ(room->enqueue(command(roomId, "two")), CommandQueueStatus::Success);
    EXPECT_EQ(room->enqueue(command(roomId, "three")), CommandQueueStatus::Full);
    ASSERT_TRUE(room->process_next()->is_accepted());
    ASSERT_TRUE(room->process_next()->is_accepted());
    EXPECT_FALSE(room->process_next().has_value());
    EXPECT_EQ(room->version().value(), VersionValue{2});
    EXPECT_EQ(room->last_event_id().value(), EventIdValue{2});
  }

  TEST(RoomParallelCommandsTest, DifferentRoomsKeepIndependentStreams)
  {
    const RoomId firstId{"concurrency/first"};
    const RoomId secondId{"concurrency/second"};
    const auto first = make_room(firstId);
    const auto second = make_room(secondId);
    std::barrier start{3};
    std::thread firstWorker{[&] { start.arrive_and_wait(); for (int index = 0; index < 12; ++index) { EXPECT_TRUE(first->execute(command(firstId, "first-" + std::to_string(index))).is_accepted()); } }};
    std::thread secondWorker{[&] { start.arrive_and_wait(); for (int index = 0; index < 12; ++index) { EXPECT_TRUE(second->execute(command(secondId, "second-" + std::to_string(index))).is_accepted()); } }};
    start.arrive_and_wait();
    firstWorker.join();
    secondWorker.join();
    EXPECT_EQ(first->version().value(), VersionValue{12});
    EXPECT_EQ(second->version().value(), VersionValue{12});
    EXPECT_EQ(first->last_event_id().value(), EventIdValue{12});
    EXPECT_EQ(second->last_event_id().value(), EventIdValue{12});
  }
} // namespace vix::realtime
