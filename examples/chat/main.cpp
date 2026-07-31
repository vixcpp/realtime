/**
 *
 * @file main.cpp
 * @author Gaspard Kirira
 * @brief Demonstrates a persistent chat room with sessions, events, history, and restoration.
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

#include <cstddef>
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
    class ChatState final : public RoomState
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
            "chat.message_posted")
        {
          return;
        }

        const auto payload =
            vix::json::to_json(
                event.payload());

        ++messageCount_;

        lastSender_ =
            payload.at("sender")
                .get<std::string>();

        lastText_ =
            payload.at("text")
                .get<std::string>();
      }

      [[nodiscard]] JsonObject
      serialize() const override
      {
        JsonObject state;

        state.set_i64(
            "messageCount",
            static_cast<std::int64_t>(
                messageCount_));

        state.set_string(
            "lastSender",
            lastSender_);

        state.set_string(
            "lastText",
            lastText_);

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
              "unsupported chat state schema"};
        }

        const auto json =
            vix::json::to_json(
                state);

        messageCount_ =
            static_cast<std::size_t>(
                json.at("messageCount")
                    .get<std::int64_t>());

        lastSender_ =
            json.at("lastSender")
                .get<std::string>();

        lastText_ =
            json.at("lastText")
                .get<std::string>();
      }

      [[nodiscard]] std::unique_ptr<RoomState>
      clone() const override
      {
        return std::make_unique<ChatState>(
            *this);
      }

      [[nodiscard]] std::size_t
      message_count() const noexcept
      {
        return messageCount_;
      }

      [[nodiscard]] const std::string &
      last_sender() const noexcept
      {
        return lastSender_;
      }

      [[nodiscard]] const std::string &
      last_text() const noexcept
      {
        return lastText_;
      }

    private:
      std::size_t messageCount_{0};

      std::string lastSender_{};
      std::string lastText_{};
    };

    class ChatHandler final : public RoomHandler
    {
    public:
      [[nodiscard]] CommandResult handle_command(
          const RoomCommand &command,
          const RoomState &,
          const RoomContext &) override
      {
        if (command.type() !=
            "chat.post_message")
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "unsupported chat command");
        }

        const auto payload =
            vix::json::to_json(
                command.payload());

        if (!payload.contains("text") ||
            !payload.at("text")
                 .is_string())
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "message text is required");
        }

        const std::string text =
            payload.at("text")
                .get<std::string>();

        if (text.empty())
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "message text cannot be empty");
        }

        if (text.size() > 500)
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "message text exceeds 500 characters");
        }

        JsonObject eventPayload;

        eventPayload.set_string(
            "sender",
            command.session_id()
                .value());

        eventPayload.set_string(
            "text",
            text);

        RoomEvent event{
            command.room_id(),
            "chat.message_posted",
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

    [[nodiscard]] RoomCommand make_message(
        const RoomId &roomId,
        const SessionId &sessionId,
        std::string text,
        std::string requestId)
    {
      JsonObject payload;

      payload.set_string(
          "text",
          std::move(text));

      RoomCommand command{
          roomId,
          sessionId,
          "chat.post_message",
          std::move(payload),
          std::move(requestId)};

      command.set_correlation_id(
          CorrelationId{
              "chat-example"});

      return command;
    }

    [[nodiscard]] const ChatState &
    chat_state(
        const Room &room)
    {
      const auto *state =
          dynamic_cast<const ChatState *>(
              &room.state());

      if (state == nullptr)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "room does not contain ChatState"};
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

    void print_result(
        const CommandResult &result)
    {
      if (result.is_rejected())
      {
        std::cout
            << "message rejected\n";

        return;
      }

      for (const RoomEvent &event :
           result.events())
      {
        const auto payload =
            vix::json::to_json(
                event.payload());

        std::cout
            << '['
            << event.event_id()
                   .value()
            << "] "
            << payload.at("sender")
                   .get<std::string>()
            << ": "
            << payload.at("text")
                   .get<std::string>()
            << " (version "
            << event.room_version()
                   .value()
            << ")\n";
      }
    }

    void print_history(
        const EventStorePtr &eventStore,
        const RoomId &roomId)
    {
      const auto events =
          eventStore->load_after(
              roomId,
              EventId{},
              100);

      std::cout
          << "\nchat history ("
          << events.size()
          << " messages)\n";

      for (const RoomEvent &event :
           events)
      {
        const auto payload =
            vix::json::to_json(
                event.payload());

        std::cout
            << '['
            << event.event_id()
                   .value()
            << "] "
            << payload.at("sender")
                   .get<std::string>()
            << ": "
            << payload.at("text")
                   .get<std::string>()
            << '\n';
      }
    }

    void print_state(
        const Room &room)
    {
      const ChatState &state =
          chat_state(
              room);

      std::cout
          << "state"
          << " messages="
          << state.message_count()
          << " lastSender="
          << state.last_sender()
          << " lastText=\""
          << state.last_text()
          << "\"\n";
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
            "chat/general"}};

    const SessionId ada{
        std::string_view{
            "ada"}};

    const SessionId linus{
        std::string_view{
            "linus"}};

    const SessionId grace{
        std::string_view{
            "grace"}};

    auto eventStore =
        std::make_shared<
            MemoryEventStore>();

    auto snapshotStore =
        std::make_shared<
            MemorySnapshotStore>();

    Config config;

    config.maxSessionsPerRoom = 32;
    config.snapshotEveryEvents = 2;
    config.snapshotOnRoomClose = true;
    config.snapshotsToKeep = 3;
    config.restoreRoomsOnOpen = true;

    Room room{
        roomId,
        std::make_unique<ChatState>(),
        std::make_unique<ChatHandler>(),
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
            linus),
        "Linus join");

    ensure_success(
        room.join(
            grace),
        "Grace join");

    std::cout
        << "chat room opened: "
        << room.id().value()
        << '\n';

    print_members(
        room);

    CommandResult result =
        room.execute(
            make_message(
                roomId,
                ada,
                "Hello everyone!",
                "request-1"));

    ensure_success(
        result,
        "Ada message");

    print_result(
        result);

    result =
        room.execute(
            make_message(
                roomId,
                linus,
                "Hello Ada, welcome to the room.",
                "request-2"));

    ensure_success(
        result,
        "Linus message");

    print_result(
        result);

    result =
        room.execute(
            make_message(
                roomId,
                grace,
                "Realtime state should stay deterministic.",
                "request-3"));

    ensure_success(
        result,
        "Grace message");

    print_result(
        result);

    print_state(
        room);

    ensure_success(
        room.leave(
            linus),
        "Linus leave");

    std::cout
        << "\nLinus left the room\n";

    print_members(
        room);

    result =
        room.execute(
            make_message(
                roomId,
                ada,
                "See you next time.",
                "request-4"));

    ensure_success(
        result,
        "Ada final message");

    print_result(
        result);

    print_history(
        eventStore,
        roomId);

    std::cout
        << "\npersisted events: "
        << eventStore->count(
               roomId)
        << '\n';

    std::cout
        << "room version: "
        << room.version()
               .value()
        << '\n';

    ensure_success(
        room.close(),
        "room close");

    const auto finalSnapshot =
        snapshotStore->load_latest(
            roomId);

    if (finalSnapshot.has_value())
    {
      const auto state =
          vix::json::to_json(
              finalSnapshot->state());

      std::cout
          << "snapshot"
          << " version="
          << finalSnapshot
                 ->room_version()
                 .value()
          << " lastEventId="
          << finalSnapshot
                 ->last_event_id()
                 .value()
          << " messages="
          << state.at("messageCount")
                 .get<std::int64_t>()
          << '\n';
    }

    Room restoredRoom{
        roomId,
        std::make_unique<ChatState>(),
        std::make_unique<ChatHandler>(),
        eventStore,
        snapshotStore,
        config};

    ensure_success(
        restoredRoom.open(),
        "restored room open");

    std::cout
        << "\nrestored room\n";

    print_state(
        restoredRoom);

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

    ensure_success(
        restoredRoom.close(),
        "restored room close");

    return 0;
  }
  catch (const std::exception &error)
  {
    std::cerr
        << "chat example failed: "
        << error.what()
        << '\n';

    return 1;
  }
}
