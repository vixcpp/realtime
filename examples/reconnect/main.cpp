/**
 *
 * @file main.cpp
 * @author Gaspard Kirira
 * @brief Demonstrates resume tokens, event replay, and snapshot fallback after reconnection.
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
#include <vector>

namespace vix::realtime
{
  namespace
  {
    class ScoreState final : public RoomState
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
            "score.added")
        {
          return;
        }

        const auto payload =
            vix::json::to_json(
                event.payload());

        score_ +=
            payload.at("points")
                .get<std::int64_t>();
      }

      [[nodiscard]] JsonObject
      serialize() const override
      {
        JsonObject state;

        state.set_i64(
            "score",
            score_);

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
              "unsupported score state schema"};
        }

        const auto json =
            vix::json::to_json(
                state);

        score_ =
            json.at("score")
                .get<std::int64_t>();
      }

      [[nodiscard]] std::unique_ptr<RoomState>
      clone() const override
      {
        return std::make_unique<ScoreState>(
            *this);
      }

      [[nodiscard]] std::int64_t
      score() const noexcept
      {
        return score_;
      }

    private:
      std::int64_t score_{0};
    };

    class ScoreHandler final : public RoomHandler
    {
    public:
      [[nodiscard]] CommandResult handle_command(
          const RoomCommand &command,
          const RoomState &,
          const RoomContext &) override
      {
        if (command.type() !=
            "score.add")
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "unsupported score command");
        }

        const auto payload =
            vix::json::to_json(
                command.payload());

        if (!payload.contains("points") ||
            !payload.at("points")
                 .is_number_integer())
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "points must be an integer");
        }

        const std::int64_t points =
            payload.at("points")
                .get<std::int64_t>();

        if (points <= 0)
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "points must be positive");
        }

        JsonObject eventPayload;

        eventPayload.set_i64(
            "points",
            points);

        RoomEvent event{
            command.room_id(),
            "score.added",
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

    struct ClientResumeState
    {
      ResumeToken token{};
      EventId lastEventId{};
    };

    [[nodiscard]] RoomCommand make_command(
        const RoomId &roomId,
        const SessionId &sessionId,
        std::int64_t points,
        std::size_t index)
    {
      JsonObject payload;

      payload.set_i64(
          "points",
          points);

      RoomCommand command{
          roomId,
          sessionId,
          "score.add",
          std::move(payload),
          "request-" +
              std::to_string(
                  index)};

      command.set_correlation_id(
          CorrelationId{
              "reconnect-example"});

      return command;
    }

    [[nodiscard]] const ScoreState &
    score_state(
        const Room &room)
    {
      const auto *state =
          dynamic_cast<const ScoreState *>(
              &room.state());

      if (state == nullptr)
      {
        throw Error{
            ErrorCode::CorruptedState,
            "room does not contain ScoreState"};
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

    void execute_score_command(
        Room &room,
        const RoomId &roomId,
        const SessionId &sessionId,
        std::int64_t points,
        std::size_t index)
    {
      const CommandResult result =
          room.execute(
              make_command(
                  roomId,
                  sessionId,
                  points,
                  index));

      ensure_success(
          result,
          "score command");

      for (const RoomEvent &event :
           result.events())
      {
        const auto payload =
            vix::json::to_json(
                event.payload());

        std::cout
            << "live event"
            << " id="
            << event.event_id()
                   .value()
            << " version="
            << event.room_version()
                   .value()
            << " points="
            << payload.at("points")
                   .get<std::int64_t>()
            << '\n';
      }
    }

    void print_replayed_events(
        const std::vector<RoomEvent> &events)
    {
      for (const RoomEvent &event :
           events)
      {
        const auto payload =
            vix::json::to_json(
                event.payload());

        std::cout
            << "replayed event"
            << " id="
            << event.event_id()
                   .value()
            << " version="
            << event.room_version()
                   .value()
            << " points="
            << payload.at("points")
                   .get<std::int64_t>()
            << '\n';
      }
    }

    void replay_from_cursor(
        const EventStorePtr &eventStore,
        const RoomId &roomId,
        ClientResumeState &client)
    {
      const auto events =
          eventStore->load_after(
              roomId,
              client.lastEventId,
              100);

      std::cout
          << "short replay contains "
          << events.size()
          << " event(s)\n";

      print_replayed_events(
          events);

      if (!events.empty())
      {
        client.lastEventId =
            events.back()
                .event_id();
      }
    }

    void replay_with_snapshot_fallback(
        const EventStorePtr &eventStore,
        const SnapshotStorePtr &snapshotStore,
        const RoomId &roomId,
        ClientResumeState &client,
        std::size_t maximumReplayEvents)
    {
      const auto missingEvents =
          eventStore->load_after(
              roomId,
              client.lastEventId,
              maximumReplayEvents + 1);

      if (missingEvents.size() <=
          maximumReplayEvents)
      {
        std::cout
            << "snapshot fallback is not required\n";

        print_replayed_events(
            missingEvents);

        if (!missingEvents.empty())
        {
          client.lastEventId =
              missingEvents.back()
                  .event_id();
        }

        return;
      }

      const auto snapshot =
          snapshotStore->load_latest(
              roomId);

      if (!snapshot.has_value())
      {
        throw Error{
            ErrorCode::CorruptedState,
            "replay is too large and no snapshot is available"};
      }

      const auto state =
          vix::json::to_json(
              snapshot->state());

      std::cout
          << "snapshot fallback"
          << " version="
          << snapshot->room_version()
                 .value()
          << " lastEventId="
          << snapshot->last_event_id()
                 .value()
          << " score="
          << state.at("score")
                 .get<std::int64_t>()
          << '\n';

      client.lastEventId =
          snapshot->last_event_id();

      const auto eventsAfterSnapshot =
          eventStore->load_after(
              roomId,
              client.lastEventId,
              100);

      print_replayed_events(
          eventsAfterSnapshot);

      if (!eventsAfterSnapshot.empty())
      {
        client.lastEventId =
            eventsAfterSnapshot.back()
                .event_id();
      }
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
            "game/score"}};

    const SessionId sessionId{
        std::string_view{
            "player-42"}};

    const ResumeToken expectedToken{
        "resume-player-42"};

    auto eventStore =
        std::make_shared<
            MemoryEventStore>();

    auto snapshotStore =
        std::make_shared<
            MemorySnapshotStore>();

    Config config;

    config.snapshotEveryEvents = 3;
    config.snapshotOnRoomClose = true;
    config.snapshotsToKeep = 3;
    config.restoreRoomsOnOpen = true;

    Room room{
        roomId,
        std::make_unique<ScoreState>(),
        std::make_unique<ScoreHandler>(),
        eventStore,
        snapshotStore,
        config};

    ensure_success(
        room.open(),
        "room open");

    ClientResumeState client{
        expectedToken,
        EventId{}};

    std::cout
        << "client connected\n";

    execute_score_command(
        room,
        roomId,
        sessionId,
        2,
        1);

    execute_score_command(
        room,
        roomId,
        sessionId,
        3,
        2);

    client.lastEventId =
        room.last_event_id();

    std::cout
        << "client acknowledged event "
        << client.lastEventId.value()
        << '\n';

    std::cout
        << "\nclient disconnected\n";

    execute_score_command(
        room,
        roomId,
        sessionId,
        4,
        3);

    execute_score_command(
        room,
        roomId,
        sessionId,
        5,
        4);

    execute_score_command(
        room,
        roomId,
        sessionId,
        6,
        5);

    std::cout
        << "authoritative score while offline: "
        << score_state(room).score()
        << '\n';

    const ResumeToken presentedToken{
        "resume-player-42"};

    if (presentedToken !=
        expectedToken)
    {
      throw Error{
          ErrorCode::CommandRejected,
          "invalid resume token"};
    }

    std::cout
        << "\nclient reconnected with a valid token\n";

    replay_from_cursor(
        eventStore,
        roomId,
        client);

    std::cout
        << "client caught up to event "
        << client.lastEventId.value()
        << '\n';

    ClientResumeState staleClient{
        expectedToken,
        EventId{}};

    std::cout
        << "\nstale client reconnecting\n";

    replay_with_snapshot_fallback(
        eventStore,
        snapshotStore,
        roomId,
        staleClient,
        2);

    std::cout
        << "stale client caught up to event "
        << staleClient.lastEventId.value()
        << '\n';

    std::cout
        << "\npersisted events: "
        << eventStore->count(
               roomId)
        << '\n';

    ensure_success(
        room.close(),
        "room close");

    Room restoredRoom{
        roomId,
        std::make_unique<ScoreState>(),
        std::make_unique<ScoreHandler>(),
        eventStore,
        snapshotStore,
        config};

    ensure_success(
        restoredRoom.open(),
        "restored room open");

    std::cout
        << "restored authoritative score: "
        << score_state(
               restoredRoom)
               .score()
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

    ensure_success(
        restoredRoom.close(),
        "restored room close");

    return 0;
  }
  catch (const std::exception &error)
  {
    std::cerr
        << "reconnect example failed: "
        << error.what()
        << '\n';

    return 1;
  }
}
