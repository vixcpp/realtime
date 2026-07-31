/**
 *
 * @file websocket_adapter_command_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for command routing through the Vix Realtime WebSocket adapter.
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
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/json/json.hpp>
#include <vix/websocket/server.hpp>

#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/transport.hpp>
#include <vix/realtime/types.hpp>
#include <vix/realtime/websocket_adapter.hpp>

namespace vix::realtime
{
  namespace
  {
    template <typename>
    inline constexpr bool dependentFalse = false;

    class TemporaryWebSocketConfig
    {
    public:
      TemporaryWebSocketConfig()
          : path_(
                make_path())
      {
        std::ofstream output{
            path_};

        output
            << "WEBSOCKET_HOST=127.0.0.1\n"
            << "WEBSOCKET_PORT=0\n"
            << "WEBSOCKET_IDLE_TIMEOUT=0\n"
            << "WEBSOCKET_PING_INTERVAL=0\n";
      }

      ~TemporaryWebSocketConfig()
      {
        std::error_code error;

        std::filesystem::remove(
            path_,
            error);
      }

      TemporaryWebSocketConfig(
          const TemporaryWebSocketConfig &) =
          delete;

      TemporaryWebSocketConfig &
      operator=(
          const TemporaryWebSocketConfig &) =
          delete;

      [[nodiscard]] const std::filesystem::path &
      path() const noexcept
      {
        return path_;
      }

    private:
      [[nodiscard]] static std::filesystem::path
      make_path()
      {
        static std::atomic<std::size_t>
            nextIdentifier{0};

        const std::size_t identifier =
            nextIdentifier.fetch_add(
                1,
                std::memory_order_relaxed);

        return std::filesystem::temp_directory_path() /
               ("vix-realtime-command-" +
                std::to_string(
                    identifier) +
                ".env");
      }

      std::filesystem::path path_{};
    };

    class RecordingConnection final : public Connection
    {
    public:
      explicit RecordingConnection(
          std::string identifier)
          : identifier_(
                std::move(identifier))
      {
      }

      [[nodiscard]] const ConnectionId &
      id() const noexcept override
      {
        return identifier_;
      }

      [[nodiscard]] bool
      is_open() const noexcept override
      {
        return open_;
      }

      void send(
          const protocol::Envelope &envelope) override
      {
        sentEnvelopes_.push_back(
            envelope);
      }

      void close(
          ErrorCode code,
          std::string_view reason) override
      {
        open_ = false;
        closeCode_ = code;
        closeReason_ = reason;
      }

      [[nodiscard]] JsonObject
      metadata() const noexcept override
      {
        return metadata_;
      }

      [[nodiscard]] std::size_t
      send_count() const noexcept
      {
        return sentEnvelopes_.size();
      }

      [[nodiscard]] const protocol::Envelope &
      sent_envelope(
          std::size_t index) const
      {
        return sentEnvelopes_.at(
            index);
      }

    private:
      ConnectionId identifier_{};
      bool open_{true};

      std::optional<ErrorCode>
          closeCode_{};

      std::string closeReason_{};
      JsonObject metadata_{};

      std::vector<protocol::Envelope>
          sentEnvelopes_{};
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
        if (command.type() ==
            "counter.reject")
        {
          return CommandResult::rejected(
              ErrorCode::CommandRejected,
              "counter command rejected");
        }

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
              "amount is required");
        }

        JsonObject eventPayload;

        eventPayload.set_i64(
            "amount",
            payload.at("amount")
                .get<std::int64_t>());

        const std::string eventType =
            command.type() ==
                    "counter.increment"
                ? "counter.incremented"
                : "counter.decremented";

        RoomEvent event{
            command.room_id(),
            eventType,
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

    class WebSocketAdapterCommandTest
        : public ::testing::Test
    {
    protected:
      WebSocketAdapterCommandTest()
          : config_(
                temporaryConfig_
                    .path()
                    .string()),
            executor_(
                std::make_shared<
                    vix::executor::RuntimeExecutor>(
                    1)),
            server_(
                config_,
                executor_)
      {
      }

      TemporaryWebSocketConfig
          temporaryConfig_{};

      vix::config::Config config_;

      std::shared_ptr<
          vix::executor::RuntimeExecutor>
          executor_;

      vix::websocket::Server server_;
    };

    struct CommandBridge
    {
      std::shared_ptr<Room> room{};
      std::optional<CommandResult>
          lastResult{};

      std::size_t receivedCount{0};
      std::size_t ignoredCount{0};

      void receive(
          const ConnectionPtr &connection,
          const protocol::Envelope &envelope)
      {
        if (envelope.kind() !=
                protocol::MessageKind::Command ||
            envelope.type() !=
                "room.command")
        {
          ++ignoredCount;
          return;
        }

        if (!envelope.room_id()
                 .has_value() ||
            *envelope.room_id() !=
                room->id())
        {
          ++ignoredCount;
          return;
        }

        ++receivedCount;

        const auto payload =
            vix::json::to_json(
                envelope.payload());

        if (!payload.contains(
                "commandType") ||
            !payload.at("commandType")
                 .is_string() ||
            !payload.contains("amount") ||
            !payload.at("amount")
                 .is_number_integer())
        {
          lastResult =
              CommandResult::rejected(
                  ErrorCode::InvalidCommand,
                  "invalid room.command payload");

          return;
        }

        JsonObject commandPayload;

        commandPayload.set_i64(
            "amount",
            payload.at("amount")
                .get<std::int64_t>());

        RoomCommand command{
            room->id(),
            SessionId{
                std::string_view{
                    "session-42"}},
            payload.at("commandType")
                .get<std::string>(),
            std::move(commandPayload),
            envelope.request_id()};

        command.set_correlation_id(
            envelope.correlation_id());

        lastResult =
            dispatch_command(
                *room,
                command);

        if (!lastResult->is_accepted())
        {
          return;
        }

        for (const RoomEvent &event :
             lastResult->events())
        {
          connection->send(
              protocol::from_event(
                  event));
        }
      }
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

    [[nodiscard]] std::shared_ptr<Room>
    make_room()
    {
      auto room =
          std::make_shared<Room>(
              make_room_id(),
              std::make_unique<CounterState>(),
              std::make_unique<CounterHandler>(),
              std::make_shared<
                  MemoryEventStore>(),
              std::make_shared<
                  MemorySnapshotStore>(),
              Config{});

      room->open();

      return room;
    }

    [[nodiscard]] RoomCommand
    make_transport_command(
        std::string commandType,
        std::int64_t amount,
        RoomId roomId =
            make_room_id())
    {
      JsonObject payload;

      payload.set_string(
          "commandType",
          std::move(commandType));

      payload.set_i64(
          "amount",
          amount);

      RoomCommand command{
          std::move(roomId),
          SessionId{
              std::string_view{
                  "session-42"}},
          "room.command",
          std::move(payload),
          RequestId{
              "request-42"}};

      command.set_correlation_id(
          CorrelationId{
              "correlation-84"});

      return command;
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

    void deliver(
        WebSocketAdapter &adapter,
        const ConnectionPtr &connection,
        const protocol::Envelope &envelope)
    {
      ASSERT_TRUE(
          static_cast<bool>(
              adapter.handlers()
                  .onEnvelope));

      adapter.handlers()
          .onEnvelope(
              connection,
              envelope);
    }

    TEST_F(
        WebSocketAdapterCommandTest,
        RoutesRoomCommandToRoom)
    {
      WebSocketAdapter adapter{
          server_};

      CommandBridge bridge{
          make_room()};

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&bridge](
              ConnectionPtr connection,
              const protocol::Envelope &envelope)
      {
        bridge.receive(
            connection,
            envelope);
      };

      adapter.set_handlers(
          std::move(handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      deliver(
          adapter,
          connection,
          protocol::from_command(
              make_transport_command(
                  "counter.increment",
                  5)));

      EXPECT_EQ(
          bridge.receivedCount,
          1U);

      ASSERT_TRUE(
          bridge.lastResult
              .has_value());

      EXPECT_TRUE(
          bridge.lastResult
              ->is_accepted());

      EXPECT_EQ(
          counter_state(
              *bridge.room)
              .value(),
          std::int64_t{5});
    }

    TEST_F(
        WebSocketAdapterCommandTest,
        SendsProducedEventToConnection)
    {
      WebSocketAdapter adapter{
          server_};

      CommandBridge bridge{
          make_room()};

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&bridge](
              ConnectionPtr connection,
              const protocol::Envelope &envelope)
      {
        bridge.receive(
            connection,
            envelope);
      };

      adapter.set_handlers(
          std::move(handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      deliver(
          adapter,
          connection,
          protocol::from_command(
              make_transport_command(
                  "counter.increment",
                  5)));

      ASSERT_EQ(
          connection->send_count(),
          1U);

      const protocol::Envelope &event =
          connection->sent_envelope(
              0);

      EXPECT_EQ(
          event.kind(),
          protocol::MessageKind::Event);

      EXPECT_EQ(
          event.type(),
          "counter.incremented");

      ASSERT_TRUE(
          event.event_id()
              .has_value());

      EXPECT_EQ(
          event.event_id()
              ->value(),
          EventIdValue{1});

      ASSERT_TRUE(
          event.room_version()
              .has_value());

      EXPECT_EQ(
          event.room_version()
              ->value(),
          VersionValue{1});
    }

    TEST_F(
        WebSocketAdapterCommandTest,
        PreservesRequestTracing)
    {
      WebSocketAdapter adapter{
          server_};

      CommandBridge bridge{
          make_room()};

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&bridge](
              ConnectionPtr connection,
              const protocol::Envelope &envelope)
      {
        bridge.receive(
            connection,
            envelope);
      };

      adapter.set_handlers(
          std::move(handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      deliver(
          adapter,
          connection,
          protocol::from_command(
              make_transport_command(
                  "counter.increment",
                  5)));

      ASSERT_EQ(
          connection->send_count(),
          1U);

      EXPECT_EQ(
          connection->sent_envelope(
                        0)
              .request_id(),
          "request-42");

      EXPECT_EQ(
          connection->sent_envelope(
                        0)
              .correlation_id(),
          "correlation-84");
    }

    TEST_F(
        WebSocketAdapterCommandTest,
        RejectedCommandDoesNotSendEvent)
    {
      WebSocketAdapter adapter{
          server_};

      CommandBridge bridge{
          make_room()};

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&bridge](
              ConnectionPtr connection,
              const protocol::Envelope &envelope)
      {
        bridge.receive(
            connection,
            envelope);
      };

      adapter.set_handlers(
          std::move(handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      deliver(
          adapter,
          connection,
          protocol::from_command(
              make_transport_command(
                  "counter.reject",
                  5)));

      ASSERT_TRUE(
          bridge.lastResult
              .has_value());

      EXPECT_TRUE(
          bridge.lastResult
              ->is_rejected());

      EXPECT_EQ(
          bridge.lastResult
              ->error_code(),
          ErrorCode::CommandRejected);

      EXPECT_EQ(
          connection->send_count(),
          0U);

      EXPECT_EQ(
          counter_state(
              *bridge.room)
              .value(),
          std::int64_t{0});
    }

    TEST_F(
        WebSocketAdapterCommandTest,
        IgnoresCommandForDifferentRoom)
    {
      WebSocketAdapter adapter{
          server_};

      CommandBridge bridge{
          make_room()};

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&bridge](
              ConnectionPtr connection,
              const protocol::Envelope &envelope)
      {
        bridge.receive(
            connection,
            envelope);
      };

      adapter.set_handlers(
          std::move(handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      deliver(
          adapter,
          connection,
          protocol::from_command(
              make_transport_command(
                  "counter.increment",
                  5,
                  make_other_room_id())));

      EXPECT_EQ(
          bridge.receivedCount,
          0U);

      EXPECT_EQ(
          bridge.ignoredCount,
          1U);

      EXPECT_FALSE(
          bridge.lastResult
              .has_value());

      EXPECT_EQ(
          connection->send_count(),
          0U);
    }

    TEST_F(
        WebSocketAdapterCommandTest,
        IgnoresNonRoomCommandEnvelope)
    {
      WebSocketAdapter adapter{
          server_};

      CommandBridge bridge{
          make_room()};

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&bridge](
              ConnectionPtr connection,
              const protocol::Envelope &envelope)
      {
        bridge.receive(
            connection,
            envelope);
      };

      adapter.set_handlers(
          std::move(handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      RoomCommand command{
          make_room_id(),
          SessionId{
              std::string_view{
                  "session-42"}},
          "room.join",
          JsonObject{},
          RequestId{
              "request-42"}};

      deliver(
          adapter,
          connection,
          protocol::from_command(
              command));

      EXPECT_EQ(
          bridge.receivedCount,
          0U);

      EXPECT_EQ(
          bridge.ignoredCount,
          1U);

      EXPECT_EQ(
          counter_state(
              *bridge.room)
              .value(),
          std::int64_t{0});
    }

    TEST_F(
        WebSocketAdapterCommandTest,
        ProcessesSequentialCommandsAgainstLatestState)
    {
      WebSocketAdapter adapter{
          server_};

      CommandBridge bridge{
          make_room()};

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&bridge](
              ConnectionPtr connection,
              const protocol::Envelope &envelope)
      {
        bridge.receive(
            connection,
            envelope);
      };

      adapter.set_handlers(
          std::move(handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      deliver(
          adapter,
          connection,
          protocol::from_command(
              make_transport_command(
                  "counter.increment",
                  10)));

      deliver(
          adapter,
          connection,
          protocol::from_command(
              make_transport_command(
                  "counter.decrement",
                  3)));

      EXPECT_EQ(
          bridge.receivedCount,
          2U);

      EXPECT_EQ(
          counter_state(
              *bridge.room)
              .value(),
          std::int64_t{7});

      EXPECT_EQ(
          bridge.room
              ->version()
              .value(),
          VersionValue{2});

      EXPECT_EQ(
          connection->send_count(),
          2U);
    }

  } // namespace

} // namespace vix::realtime
