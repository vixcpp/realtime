/**
 *
 * @file room_events_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for event persistence and application in Vix Realtime rooms.
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

    struct AppliedEvent
    {
      std::string type{};
      EventId eventId{};
      RoomVersion roomVersion{};
      EventAudience audience{
          EventAudience::Room};
    };

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
        else
        {
          throw Error{
              ErrorCode::EventApplyFailure,
              "unsupported counter event"};
        }

        applied_.push_back(
            AppliedEvent{
                event.type(),
                event.event_id(),
                event.room_version(),
                event.audience()});
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

        applied_.clear();
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

      [[nodiscard]] const std::vector<AppliedEvent> &
      applied_events() const noexcept
      {
        return applied_;
      }

    private:
      std::int64_t value_{0};
      std::vector<AppliedEvent> applied_{};
    };

    class EventHandler final : public RoomHandler
    {
    public:
      [[nodiscard]] CommandResult handle_command(
          const RoomCommand &command,
          const RoomState &,
          const RoomContext &) override
      {
        if (command.type() ==
            "counter.reject")
        {
          return CommandResult::rejected(
              ErrorCode::CommandRejected,
              "command rejected");
        }

        if (command.type() ==
            "counter.increment")
        {
          return CommandResult::accepted(
              {make_event(
                  command,
                  "counter.incremented",
                  amount_from(
                      command),
                  EventAudience::Room)});
        }

        if (command.type() ==
            "counter.sequence")
        {
          return CommandResult::accepted(
              {
                  make_event(
                      command,
                      "counter.incremented",
                      10,
                      EventAudience::Room),
                  make_event(
                      command,
                      "counter.decremented",
                      3,
                      EventAudience::Room),
              });
        }

        if (command.type() ==
            "counter.sender")
        {
          return CommandResult::accepted(
              {make_event(
                  command,
                  "counter.incremented",
                  amount_from(
                      command),
                  EventAudience::Sender)});
        }

        return CommandResult::rejected(
            ErrorCode::InvalidCommand,
            "unsupported command");
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
          std::int64_t amount,
          EventAudience audience)
      {
        JsonObject payload;

        payload.set_i64(
            "amount",
            amount);

        RoomEvent event{
            command.room_id(),
            std::move(type),
            std::move(payload),
            audience};

        event
            .set_source_session(
                command.session_id())
            .set_request_id(
                command.request_id())
            .set_correlation_id(
                command.correlation_id());

        return event;
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

    [[nodiscard]] RoomFixture make_fixture()
    {
      RoomFixture fixture;

      fixture.room =
          std::make_unique<Room>(
              make_room_id(),
              std::make_unique<
                  CounterState>(),
              std::make_unique<
                  EventHandler>(),
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

    TEST(RoomEventsTest, PersistsAcceptedEvent)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.increment",
                  5));

      ASSERT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          1U);

      const std::vector<RoomEvent> stored =
          fixture.eventStore->load_after(
              make_room_id(),
              EventId{},
              100);

      ASSERT_EQ(
          stored.size(),
          1U);

      EXPECT_EQ(
          stored.front().type(),
          "counter.incremented");
    }

    TEST(RoomEventsTest, AssignsEventIdentifierBeforeApplication)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.increment",
                  5));

      ASSERT_TRUE(
          result.is_accepted());

      const auto &applied =
          counter_state(
              *fixture.room)
              .applied_events();

      ASSERT_EQ(
          applied.size(),
          1U);

      EXPECT_EQ(
          applied.front()
              .eventId
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          applied.front()
              .roomVersion
              .value(),
          VersionValue{1});
    }

    TEST(RoomEventsTest, ReturnedEventMatchesPersistedEvent)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.increment",
                  5));

      ASSERT_TRUE(
          result.is_accepted());

      ASSERT_EQ(
          result.events().size(),
          1U);

      const std::vector<RoomEvent> stored =
          fixture.eventStore->load_after(
              make_room_id(),
              EventId{},
              100);

      ASSERT_EQ(
          stored.size(),
          1U);

      const RoomEvent &returned =
          result.events().front();

      const RoomEvent &persisted =
          stored.front();

      EXPECT_EQ(
          returned.event_id(),
          persisted.event_id());

      EXPECT_EQ(
          returned.room_version(),
          persisted.room_version());

      EXPECT_EQ(
          returned.type(),
          persisted.type());

      EXPECT_EQ(
          vix::json::to_json(
              returned.payload()),
          vix::json::to_json(
              persisted.payload()));
    }

    TEST(RoomEventsTest, AppliesPersistedEventToState)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.increment",
                  5));

      ASSERT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{5});
    }

    TEST(RoomEventsTest, PreservesEventAudience)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.sender",
                  5));

      ASSERT_TRUE(
          result.is_accepted());

      ASSERT_EQ(
          result.events().size(),
          1U);

      EXPECT_EQ(
          result.events()
              .front()
              .audience(),
          EventAudience::Sender);

      const std::vector<RoomEvent> stored =
          fixture.eventStore->load_after(
              make_room_id(),
              EventId{},
              100);

      ASSERT_EQ(
          stored.size(),
          1U);

      EXPECT_EQ(
          stored.front()
              .audience(),
          EventAudience::Sender);
    }

    TEST(RoomEventsTest, PreservesCommandTracingInformation)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.increment",
                  5));

      ASSERT_TRUE(
          result.is_accepted());

      ASSERT_EQ(
          result.events().size(),
          1U);

      const RoomEvent &event =
          result.events().front();

      ASSERT_TRUE(
          event.source_session()
              .has_value());

      EXPECT_EQ(
          *event.source_session(),
          make_session_id());

      EXPECT_EQ(
          event.request_id(),
          "request-42");

      EXPECT_EQ(
          event.correlation_id(),
          "correlation-84");
    }

    TEST(RoomEventsTest, PersistsMultipleEventsAtomically)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.sequence"));

      ASSERT_TRUE(
          result.is_accepted());

      ASSERT_EQ(
          result.events().size(),
          2U);

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          2U);

      EXPECT_EQ(
          result.events()[0]
              .event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          result.events()[1]
              .event_id()
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          result.events()[0]
              .room_version()
              .value(),
          VersionValue{1});

      EXPECT_EQ(
          result.events()[1]
              .room_version()
              .value(),
          VersionValue{2});
    }

    TEST(RoomEventsTest, AppliesMultipleEventsInOrder)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.sequence"));

      ASSERT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{7});

      const auto &applied =
          counter_state(
              *fixture.room)
              .applied_events();

      ASSERT_EQ(
          applied.size(),
          2U);

      EXPECT_EQ(
          applied[0].type,
          "counter.incremented");

      EXPECT_EQ(
          applied[1].type,
          "counter.decremented");

      EXPECT_EQ(
          applied[0]
              .eventId
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          applied[1]
              .eventId
              .value(),
          EventIdValue{2});
    }

    TEST(RoomEventsTest, UpdatesRoomPositionToLastEventInBatch)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.sequence"));

      ASSERT_TRUE(
          result.is_accepted());

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
    }

    TEST(RoomEventsTest, SequentialCommandsContinueEventSequence)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult first =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.increment",
                  5));

      const CommandResult second =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.increment",
                  3));

      ASSERT_TRUE(
          first.is_accepted());

      ASSERT_TRUE(
          second.is_accepted());

      ASSERT_EQ(
          second.events().size(),
          1U);

      EXPECT_EQ(
          second.events()
              .front()
              .event_id()
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          second.events()
              .front()
              .room_version()
              .value(),
          VersionValue{2});

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{8});
    }

    TEST(RoomEventsTest, RejectedCommandDoesNotPersistEvents)
    {
      RoomFixture fixture =
          make_fixture();

      const CommandResult result =
          dispatch_command(
              *fixture.room,
              make_command(
                  "counter.reject"));

      EXPECT_TRUE(
          result.is_rejected());

      EXPECT_TRUE(
          result.events().empty());

      EXPECT_EQ(
          fixture.eventStore->count(
              make_room_id()),
          0U);

      EXPECT_TRUE(
          fixture.room
              ->version()
              .is_initial());

      EXPECT_TRUE(
          fixture.room
              ->last_event_id()
              .empty());

      EXPECT_EQ(
          counter_state(
              *fixture.room)
              .value(),
          std::int64_t{0});
    }

  } // namespace

} // namespace vix::realtime
