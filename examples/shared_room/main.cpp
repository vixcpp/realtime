/**
 *
 * @file main.cpp
 * @author Gaspard Kirira
 * @brief Demonstrates multiple logical sessions sharing one authoritative room.
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

#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace vix::realtime
{
  namespace
  {
    class SharedRoomState final : public RoomState
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

        if (event.type() ==
            "shared.object_placed")
        {
          ++objectCount_;
        }
        else if (event.type() ==
                 "shared.object_removed")
        {
          objectCount_ =
              std::max<std::int64_t>(
                  0,
                  objectCount_ - 1);
        }
        else
        {
          return;
        }

        ++actionCount_;

        lastActor_ =
            payload.at("actor")
                .get<std::string>();

        lastAction_ =
            event.type();
      }

      [[nodiscard]] JsonObject
      serialize() const override
      {
        JsonObject state;

        state.set_i64(
            "objectCount",
            objectCount_);

        state.set_i64(
            "actionCount",
            actionCount_);

        state.set_string(
            "lastActor",
            lastActor_);

        state.set_string(
            "lastAction",
            lastAction_);

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
              "unsupported shared room state schema"};
        }

        const auto json =
            vix::json::to_json(
                state);

        objectCount_ =
            json.at("objectCount")
                .get<std::int64_t>();

        actionCount_ =
            json.at("actionCount")
                .get<std::int64_t>();

        lastActor_ =
            json.at("lastActor")
                .get<std::string>();

        lastAction_ =
            json.at("lastAction")
                .get<std::string>();
      }

      [[nodiscard]] std::unique_ptr<RoomState>
      clone() const override
      {
        return std::make_unique<
            SharedRoomState>(
            *this);
      }

      [[nodiscard]] std::int64_t
      object_count() const noexcept
      {
        return objectCount_;
      }

      [[nodiscard]] std::int64_t
      action_count() const noexcept
      {
        return actionCount_;
      }

      [[nodiscard]] const std::string &
      last_actor() const noexcept
      {
        return lastActor_;
      }

      [[nodiscard]] const std::string &
      last_action() const noexcept
      {
        return lastAction_;
      }

    private:
      std::int64_t objectCount_{0};
      std::int64_t actionCount_{0};

      std::string lastActor_{};
      std::string lastAction_{};
    };

    class SharedRoomHandler final : public RoomHandler
    {
    public:
      [[nodiscard]] CommandResult handle_command(
          const RoomCommand &command,
          const RoomState &,
          const RoomContext &) override
      {
        if (command.type() ==
            "shared.place_object")
        {
          return place_object(
              command);
        }

        if (command.type() ==
            "shared.remove_object")
        {
          return remove_object(
              command);
        }

        return CommandResult::rejected(
            ErrorCode::InvalidCommand,
            "unsupported shared room command");
      }

    private:
      [[nodiscard]] static CommandResult
      place_object(
          const RoomCommand &command)
      {
        const auto payload =
            vix::json::to_json(
                command.payload());

        if (!payload.contains("x") ||
            !payload.at("x")
                 .is_number_integer() ||
            !payload.contains("y") ||
            !payload.at("y")
                 .is_number_integer())
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "object coordinates must be integers");
        }

        const std::int64_t x =
            payload.at("x")
                .get<std::int64_t>();

        const std::int64_t y =
            payload.at("y")
                .get<std::int64_t>();

        if (x < 0 || y < 0)
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "object coordinates cannot be negative");
        }

        JsonObject eventPayload;

        eventPayload.set_string(
            "actor",
            command.session_id()
                .value());

        eventPayload.set_i64(
            "x",
            x);

        eventPayload.set_i64(
            "y",
            y);

        RoomEvent event{
            command.room_id(),
            "shared.object_placed",
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

      [[nodiscard]] static CommandResult
      remove_object(
          const RoomCommand &command)
      {
        JsonObject eventPayload;

        eventPayload.set_string(
            "actor",
            command.session_id()
                .value());

        RoomEvent event{
            command.room_id(),
            "shared.object_removed",
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

    [[nodiscard]] RoomCommand
    make_place_command(
        const RoomId &roomId,
        const SessionId &sessionId,
        std::int64_t x,
        std::int64_t y,
        std::string requestId)
    {
      JsonObject payload;

      payload.set_i64(
          "x",
          x);

      payload.set_i64(
          "y",
          y);

      RoomCommand command{
          roomId,
          sessionId,
          "shared.place_object",
          std::move(payload),
          std::move(requestId)};

      command.set_correlation_id(
          CorrelationId{
              "shared-room-example"});

      return command;
    }

    [[nodiscard]] RoomCommand
    make_remove_command(
        const RoomId &roomId,
        const SessionId &sessionId,
        std::string requestId)
    {
      RoomCommand command{
          roomId,
          sessionId,
          "shared.remove_object",
          JsonObject{},
          std::move(requestId)};

      command.set_correlation_id(
          CorrelationId{
              "shared-room-example"});

      return command;
    }

    [[nodiscard]] const SharedRoomState &
    shared_state(
        const Room &room)
    {
      const auto *state =
          dynamic_cast<
              const SharedRoomState *>(
              &room.state());

      if (state == nullptr)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "room does not contain SharedRoomState"};
      }

      return *state;
    }

    void ensure_success(
        const CommandResult &result,
        std::string_view operation)
    {
      if (result.is_rejected())
      {
        throw Error{
            ErrorCode::CommandRejected,
            std::string{
                operation} +
                " was rejected"};
      }
    }

    void print_members(
        const Room &room)
    {
      std::cout
          << "members ("
          << room.session_count()
          << "):";

      for (const SessionId &sessionId :
           room.sessions())
      {
        std::cout
            << ' '
            << sessionId.value();
      }

      std::cout
          << '\n';
    }

    void print_events(
        const CommandResult &result)
    {
      for (const RoomEvent &event :
           result.events())
      {
        const auto payload =
            vix::json::to_json(
                event.payload());

        std::cout
            << "broadcast"
            << " eventId="
            << event.event_id()
                   .value()
            << " version="
            << event.room_version()
                   .value()
            << " type="
            << event.type()
            << " actor="
            << payload.at("actor")
                   .get<std::string>();

        if (payload.contains("x") &&
            payload.contains("y"))
        {
          std::cout
              << " position=("
              << payload.at("x")
                     .get<std::int64_t>()
              << ", "
              << payload.at("y")
                     .get<std::int64_t>()
              << ')';
        }

        std::cout
            << '\n';
      }
    }

    void print_state(
        const Room &room)
    {
      const SharedRoomState &state =
          shared_state(
              room);

      std::cout
          << "shared state"
          << " objects="
          << state.object_count()
          << " actions="
          << state.action_count()
          << " lastActor="
          << state.last_actor()
          << " lastAction="
          << state.last_action()
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
            "shared/world"}};

    const SessionId ada{
        std::string_view{
            "session-ada"}};

    const SessionId bob{
        std::string_view{
            "session-bob"}};

    auto eventStore =
        std::make_shared<
            MemoryEventStore>();

    auto snapshotStore =
        std::make_shared<
            MemorySnapshotStore>();

    Config config;

    config.maxSessionsPerRoom = 8;
    config.snapshotEveryEvents = 2;
    config.snapshotOnRoomClose = true;
    config.snapshotsToKeep = 3;
    config.restoreRoomsOnOpen = true;

    Room room{
        roomId,
        std::make_unique<
            SharedRoomState>(),
        std::make_unique<
            SharedRoomHandler>(),
        eventStore,
        snapshotStore,
        config};

    ensure_success(
        room.open(),
        "room open");

    ensure_success(
        room.join(
            ada),
        "Ada join");

    ensure_success(
        room.join(
            bob),
        "Bob join");

    std::cout
        << "room opened: "
        << room.id().value()
        << '\n';

    print_members(
        room);

    CommandResult result =
        room.execute(
            make_place_command(
                roomId,
                ada,
                12,
                8,
                "request-1"));

    ensure_success(
        result,
        "Ada place object");

    print_events(
        result);

    print_state(
        room);

    result =
        room.execute(
            make_place_command(
                roomId,
                bob,
                20,
                4,
                "request-2"));

    ensure_success(
        result,
        "Bob place object");

    print_events(
        result);

    print_state(
        room);

    result =
        room.execute(
            make_remove_command(
                roomId,
                ada,
                "request-3"));

    ensure_success(
        result,
        "Ada remove object");

    print_events(
        result);

    print_state(
        room);

    ensure_success(
        room.leave(
            bob),
        "Bob leave");

    print_members(
        room);

    std::cout
        << "persisted events: "
        << eventStore->count(
               roomId)
        << '\n';

    std::cout
        << "room version: "
        << room.version()
               .value()
        << '\n';

    std::cout
        << "last event: "
        << room.last_event_id()
               .value()
        << '\n';

    ensure_success(
        room.close(),
        "room close");

    const auto snapshot =
        snapshotStore->load_latest(
            roomId);

    if (snapshot.has_value())
    {
      const auto state =
          vix::json::to_json(
              snapshot->state());

      std::cout
          << "final snapshot"
          << " version="
          << snapshot->room_version()
                 .value()
          << " objects="
          << state.at("objectCount")
                 .get<std::int64_t>()
          << '\n';
    }

    return 0;
  }
  catch (const std::exception &error)
  {
    std::cerr
        << "shared room example failed: "
        << error.what()
        << '\n';

    return 1;
  }
}
