
/**
 *
 * @file main.cpp
 * @author Gaspard Kirira
 * @brief Demonstrates commands, events, snapshots, and restoration with a realtime counter.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/json/json.hpp>
#include <vix/realtime.hpp>

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vix::realtime
{
  namespace
  {
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
        if (event.type() !=
            "counter.changed")
        {
          return;
        }

        const auto payload =
            vix::json::to_json(
                event.payload());

        value_ +=
            payload.at("delta")
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
                "counter.increment" &&
            command.type() !=
                "counter.decrement")
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
              "counter amount must be an integer");
        }

        std::int64_t delta =
            payload.at("amount")
                .get<std::int64_t>();

        if (delta <= 0)
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "counter amount must be positive");
        }

        if (command.type() ==
            "counter.decrement")
        {
          delta = -delta;
        }

        JsonObject eventPayload;

        eventPayload.set_i64(
            "delta",
            delta);

        RoomEvent event{
            command.room_id(),
            "counter.changed",
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

    [[nodiscard]] RoomCommand make_command(
        const RoomId &roomId,
        const SessionId &sessionId,
        std::string type,
        std::int64_t amount,
        std::string requestId)
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      RoomCommand command{
          roomId,
          sessionId,
          std::move(type),
          std::move(payload),
          std::move(requestId)};

      command.set_correlation_id(
          CorrelationId{
              "counter-example"});

      return command;
    }

    [[nodiscard]] const CounterState &
    counter_state(
        const Room &room)
    {
      const auto *state =
          dynamic_cast<const CounterState *>(
              &room.state());

      if (state == nullptr)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "room does not contain a counter state"};
      }

      return *state;
    }

    void print_result(
        const CommandResult &result)
    {
      if (result.is_rejected())
      {
        std::cout
            << "command rejected\n";

        return;
      }

      if (result.is_ignored())
      {
        std::cout
            << "command ignored\n";

        return;
      }

      for (const RoomEvent &event :
           result.events())
      {
        const auto payload =
            vix::json::to_json(
                event.payload());

        std::cout
            << "event"
            << " id="
            << event.event_id().value()
            << " version="
            << event.room_version().value()
            << " type="
            << event.type()
            << " delta="
            << payload.at("delta")
                   .get<std::int64_t>()
            << '\n';
      }
    }

    void print_snapshot(
        const RoomSnapshot &snapshot)
    {
      const auto state =
          vix::json::to_json(
              snapshot.state());

      std::cout
          << "snapshot"
          << " version="
          << snapshot.room_version().value()
          << " lastEventId="
          << snapshot.last_event_id().value()
          << " value="
          << state.at("value")
                 .get<std::int64_t>()
          << '\n';
    }

  } // namespace

} // namespace vix::realtime

int main()
{
  using namespace vix::realtime;

  try
  {
    const RoomId roomId{
        std::string_view{
            "counter/main"}};

    const SessionId sessionId{
        std::string_view{
            "session-demo"}};

    auto eventStore =
        std::make_shared<
            MemoryEventStore>();

    auto snapshotStore =
        std::make_shared<
            MemorySnapshotStore>();

    Config config;

    config.snapshotEveryEvents = 2;
    config.snapshotOnRoomClose = true;
    config.snapshotsToKeep = 3;
    config.restoreRoomsOnOpen = true;

    Room room{
        roomId,
        std::make_unique<CounterState>(),
        std::make_unique<CounterHandler>(),
        eventStore,
        snapshotStore,
        config};

    room.open();

    std::cout
        << "room opened: "
        << room.id().value()
        << '\n';

    print_result(
        room.execute(
            make_command(
                roomId,
                sessionId,
                "counter.increment",
                5,
                "request-1")));

    print_result(
        room.execute(
            make_command(
                roomId,
                sessionId,
                "counter.increment",
                3,
                "request-2")));

    std::cout
        << "value after two commands: "
        << counter_state(room).value()
        << '\n';

    const auto periodicSnapshot =
        snapshotStore->load_latest(
            roomId);

    if (periodicSnapshot.has_value())
    {
      print_snapshot(
          *periodicSnapshot);
    }

    print_result(
        room.execute(
            make_command(
                roomId,
                sessionId,
                "counter.decrement",
                2,
                "request-3")));

    std::cout
        << "value before close: "
        << counter_state(room).value()
        << '\n';

    std::cout
        << "persisted events: "
        << eventStore->count(
               roomId)
        << '\n';

    room.close();

    const auto finalSnapshot =
        snapshotStore->load_latest(
            roomId);

    if (finalSnapshot.has_value())
    {
      print_snapshot(
          *finalSnapshot);
    }

    Room restoredRoom{
        roomId,
        std::make_unique<CounterState>(),
        std::make_unique<CounterHandler>(),
        eventStore,
        snapshotStore,
        config};

    restoredRoom.open();

    std::cout
        << "restored value: "
        << counter_state(
               restoredRoom)
               .value()
        << '\n';

    std::cout
        << "restored version: "
        << restoredRoom
               .version()
               .value()
        << '\n';

    std::cout
        << "restored last event: "
        << restoredRoom
               .last_event_id()
               .value()
        << '\n';

    restoredRoom.close();

    return 0;
  }
  catch (const std::exception &error)
  {
    std::cerr
        << "counter example failed: "
        << error.what()
        << '\n';

    return 1;
  }
}
