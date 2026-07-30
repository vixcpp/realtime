/**
 *
 * @file room_handler_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime room command handler interface.
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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <vix/json/json.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/node_id.hpp>
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

        if (event.type() == "counter.incremented")
        {
          value_ +=
              payload.at("amount")
                  .get<std::int64_t>();
        }
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
          SchemaVersion) override
      {
        const auto json =
            vix::json::to_json(state);

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

    class OtherState final : public RoomState
    {
    public:
      [[nodiscard]] SchemaVersion
      schema_version() const noexcept override
      {
        return SchemaVersion{1};
      }

      void apply(
          const RoomEvent &) override
      {
      }

      [[nodiscard]] JsonObject
      serialize() const override
      {
        return {};
      }

      void restore(
          const JsonObject &,
          SchemaVersion) override
      {
      }

      [[nodiscard]] std::unique_ptr<RoomState>
      clone() const override
      {
        return std::make_unique<OtherState>(
            *this);
      }
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

        lastCommandType_ =
            command.type();

        lastRoomId_ =
            context.room_id();

        const auto *counterState =
            dynamic_cast<const CounterState *>(
                &state);

        if (counterState == nullptr)
        {
          return CommandResult::rejected(
              ErrorCode::CorruptedState,
              "counter handler received an incompatible state");
        }

        observedValue_ =
            counterState->value();

        if (command.type() != "counter.increment")
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "unsupported counter command");
        }

        const auto payload =
            vix::json::to_json(
                command.payload());

        const std::int64_t amount =
            payload.at("amount")
                .get<std::int64_t>();

        if (amount <= 0)
        {
          return CommandResult::rejected(
              ErrorCode::CommandRejected,
              "increment amount must be positive");
        }

        JsonObject eventPayload;

        eventPayload.set_i64(
            "amount",
            amount);

        RoomEvent event{
            command.room_id(),
            std::string{
                "counter.incremented"},
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

      [[nodiscard]] const std::string &
      last_command_type() const noexcept
      {
        return lastCommandType_;
      }

      [[nodiscard]] const RoomId &
      last_room_id() const noexcept
      {
        return lastRoomId_;
      }

      [[nodiscard]] std::int64_t
      observed_value() const noexcept
      {
        return observedValue_;
      }

    private:
      std::size_t callCount_{0};
      std::string lastCommandType_{};
      RoomId lastRoomId_{};
      std::int64_t observedValue_{0};
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

    [[nodiscard]] NodeId make_node_id()
    {
      return NodeId{
          std::string_view{
              "node-1"}};
    }

    [[nodiscard]] RoomContext make_context()
    {
      return RoomContext{
          make_room_id(),
          RoomVersion{
              VersionValue{7}},
          EventId{
              EventIdValue{7}},
          std::optional<SessionId>{
              make_session_id()},
          RequestId{
              "request-42"},
          CorrelationId{
              "correlation-84"},
          std::optional<NodeId>{
              make_node_id()},
          Timestamp{
              std::chrono::seconds{
                  1234}},
          {}};
    }

    [[nodiscard]] RoomCommand make_command(
        std::string type,
        std::int64_t amount)
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

    TEST(RoomHandlerTest, IsAbstractInterface)
    {
      EXPECT_TRUE(
          std::is_abstract_v<
              RoomHandler>);
    }

    TEST(RoomHandlerTest, HasVirtualDestructor)
    {
      EXPECT_TRUE(
          std::has_virtual_destructor_v<
              RoomHandler>);
    }

    TEST(RoomHandlerTest, CanBeUsedThroughBasePointer)
    {
      std::unique_ptr<RoomHandler> handler =
          std::make_unique<CounterHandler>();

      EXPECT_NE(
          handler,
          nullptr);
    }

    TEST(RoomHandlerTest, AcceptsSupportedCommand)
    {
      CounterHandler handler;

      const CounterState state{
          10};

      const CommandResult result =
          handler.handle_command(
              make_command(
                  "counter.increment",
                  3),
              state,
              make_context());

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::None);

      EXPECT_EQ(
          result.event_count(),
          1U);

      EXPECT_NO_THROW(
          result.validate());
    }

    TEST(RoomHandlerTest, ProducesAuthoritativeEvent)
    {
      CounterHandler handler;

      const CounterState state{
          10};

      const CommandResult result =
          handler.handle_command(
              make_command(
                  "counter.increment",
                  3),
              state,
              make_context());

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
          event.audience(),
          EventAudience::Room);

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

      const auto payload =
          vix::json::to_json(
              event.payload());

      EXPECT_EQ(
          payload.at("amount")
              .get<std::int64_t>(),
          std::int64_t{3});
    }

    TEST(RoomHandlerTest, DoesNotMutateRoomState)
    {
      CounterHandler handler;

      const CounterState state{
          10};

      const CommandResult result =
          handler.handle_command(
              make_command(
                  "counter.increment",
                  3),
              state,
              make_context());

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          state.value(),
          std::int64_t{10});

      EXPECT_EQ(
          handler.observed_value(),
          std::int64_t{10});
    }

    TEST(RoomHandlerTest, ReceivesCommandAndContext)
    {
      CounterHandler handler;

      const CounterState state{
          10};

      static_cast<void>(
          handler.handle_command(
              make_command(
                  "counter.increment",
                  3),
              state,
              make_context()));

      EXPECT_EQ(
          handler.call_count(),
          1U);

      EXPECT_EQ(
          handler.last_command_type(),
          "counter.increment");

      EXPECT_EQ(
          handler.last_room_id(),
          make_room_id());
    }

    TEST(RoomHandlerTest, RejectsUnsupportedCommand)
    {
      CounterHandler handler;

      const CounterState state{
          10};

      const CommandResult result =
          handler.handle_command(
              make_command(
                  "counter.reset",
                  1),
              state,
              make_context());

      EXPECT_TRUE(
          result.is_rejected());

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::InvalidCommand);

      EXPECT_TRUE(
          result.events().empty());

      EXPECT_NO_THROW(
          result.validate());
    }

    TEST(RoomHandlerTest, RejectsInvalidCommandPayload)
    {
      CounterHandler handler;

      const CounterState state{
          10};

      const CommandResult result =
          handler.handle_command(
              make_command(
                  "counter.increment",
                  0),
              state,
              make_context());

      EXPECT_TRUE(
          result.is_rejected());

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::CommandRejected);

      EXPECT_TRUE(
          result.events().empty());

      EXPECT_NO_THROW(
          result.validate());
    }

    TEST(RoomHandlerTest, RejectsIncompatibleRoomState)
    {
      CounterHandler handler;
      const OtherState state;

      const CommandResult result =
          handler.handle_command(
              make_command(
                  "counter.increment",
                  3),
              state,
              make_context());

      EXPECT_TRUE(
          result.is_rejected());

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::CorruptedState);

      EXPECT_TRUE(
          result.events().empty());

      EXPECT_NO_THROW(
          result.validate());
    }

  } // namespace

} // namespace vix::realtime
