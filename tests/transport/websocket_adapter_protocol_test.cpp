/**
 *
 * @file websocket_adapter_protocol_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for protocol delivery through the Vix Realtime WebSocket adapter.
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
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/json/json.hpp>
#include <vix/websocket/server.hpp>

#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/transport.hpp>
#include <vix/realtime/types.hpp>
#include <vix/realtime/websocket_adapter.hpp>

namespace vix::realtime
{
  namespace
  {
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
               ("vix-realtime-protocol-" +
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

      [[nodiscard]] std::string_view
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

      [[nodiscard]] const JsonObject &
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

      [[nodiscard]] ErrorCode
      close_code() const noexcept
      {
        return closeCode_;
      }

      [[nodiscard]] const std::string &
      close_reason() const noexcept
      {
        return closeReason_;
      }

    private:
      std::string identifier_{};
      bool open_{true};

      ErrorCode closeCode_{
          ErrorCode::None};

      std::string closeReason_{};
      JsonObject metadata_{};

      std::vector<protocol::Envelope>
          sentEnvelopes_{};
    };

    class WebSocketAdapterProtocolTest
        : public ::testing::Test
    {
    protected:
      WebSocketAdapterProtocolTest()
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

    [[nodiscard]] RoomCommand make_command()
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          5);

      RoomCommand command{
          make_room_id(),
          make_session_id(),
          "counter.increment",
          std::move(payload),
          RequestId{
              "request-42"}};

      command.set_correlation_id(
          CorrelationId{
              "correlation-84"});

      return command;
    }

    [[nodiscard]] RoomEvent make_event()
    {
      JsonObject payload;

      payload.set_i64(
          "value",
          5);

      RoomEvent event{
          make_room_id(),
          "counter.incremented",
          std::move(payload),
          EventAudience::Room};

      event
          .set_event_id(
              EventId{
                  EventIdValue{7}})
          .set_room_version(
              RoomVersion{
                  VersionValue{7}})
          .set_source_session(
              make_session_id())
          .set_request_id(
              RequestId{
                  "request-42"})
          .set_correlation_id(
              CorrelationId{
                  "correlation-84"});

      return event;
    }

    void deliver_envelope(
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
        WebSocketAdapterProtocolTest,
        DeliversCommandEnvelopeToHandler)
    {
      WebSocketAdapter adapter{
          server_};

      auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      ConnectionPtr observedConnection{};
      protocol::Envelope observedEnvelope{};
      std::size_t callCount = 0;

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&](
              ConnectionPtr receivedConnection,
              const protocol::Envelope &envelope)
      {
        ++callCount;

        observedConnection =
            std::move(
                receivedConnection);

        observedEnvelope =
            envelope;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      adapter.attach();

      const protocol::Envelope envelope =
          protocol::from_command(
              make_command());

      deliver_envelope(
          adapter,
          connection,
          envelope);

      EXPECT_EQ(
          callCount,
          1U);

      EXPECT_EQ(
          observedConnection,
          connection);

      EXPECT_EQ(
          observedEnvelope.kind(),
          protocol::MessageKind::Command);

      EXPECT_EQ(
          observedEnvelope.type(),
          "counter.increment");

      ASSERT_TRUE(
          observedEnvelope.room_id()
              .has_value());

      EXPECT_EQ(
          *observedEnvelope.room_id(),
          make_room_id());

      EXPECT_EQ(
          observedEnvelope.request_id(),
          "request-42");

      EXPECT_EQ(
          observedEnvelope.correlation_id(),
          "correlation-84");
    }

    TEST_F(
        WebSocketAdapterProtocolTest,
        PreservesCommandPayload)
    {
      WebSocketAdapter adapter{
          server_};

      auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      std::int64_t observedAmount = 0;

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&](
              ConnectionPtr,
              const protocol::Envelope &envelope)
      {
        const auto payload =
            vix::json::to_json(
                envelope.payload());

        observedAmount =
            payload.at("amount")
                .get<std::int64_t>();
      };

      adapter.set_handlers(
          std::move(
              handlers));

      deliver_envelope(
          adapter,
          connection,
          protocol::from_command(
              make_command()));

      EXPECT_EQ(
          observedAmount,
          std::int64_t{5});
    }

    TEST_F(
        WebSocketAdapterProtocolTest,
        DeliversEventEnvelopeToHandler)
    {
      WebSocketAdapter adapter{
          server_};

      auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      protocol::Envelope observedEnvelope{};

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&](
              ConnectionPtr,
              const protocol::Envelope &envelope)
      {
        observedEnvelope =
            envelope;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      deliver_envelope(
          adapter,
          connection,
          protocol::from_event(
              make_event()));

      EXPECT_EQ(
          observedEnvelope.kind(),
          protocol::MessageKind::Event);

      EXPECT_EQ(
          observedEnvelope.type(),
          "counter.incremented");

      ASSERT_TRUE(
          observedEnvelope.event_id()
              .has_value());

      EXPECT_EQ(
          observedEnvelope.event_id()
              ->value(),
          EventIdValue{7});

      ASSERT_TRUE(
          observedEnvelope.room_version()
              .has_value());

      EXPECT_EQ(
          observedEnvelope.room_version()
              ->value(),
          VersionValue{7});
    }

    TEST_F(
        WebSocketAdapterProtocolTest,
        ProtocolRoundTripPreservesEnvelope)
    {
      WebSocketAdapter adapter{
          server_};

      auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      protocol::Envelope observedEnvelope{};

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&](
              ConnectionPtr,
              const protocol::Envelope &envelope)
      {
        observedEnvelope =
            envelope;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const protocol::Envelope original =
          protocol::from_command(
              make_command());

      const std::string serialized =
          protocol::serialize(
              original);

      const protocol::Envelope parsed =
          protocol::parse(
              serialized);

      deliver_envelope(
          adapter,
          connection,
          parsed);

      EXPECT_EQ(
          observedEnvelope.kind(),
          original.kind());

      EXPECT_EQ(
          observedEnvelope.type(),
          original.type());

      EXPECT_EQ(
          observedEnvelope.room_id(),
          original.room_id());

      EXPECT_EQ(
          observedEnvelope.request_id(),
          original.request_id());

      EXPECT_EQ(
          observedEnvelope.correlation_id(),
          original.correlation_id());

      EXPECT_EQ(
          vix::json::to_json(
              observedEnvelope.payload()),
          vix::json::to_json(
              original.payload()));
    }

    TEST_F(
        WebSocketAdapterProtocolTest,
        HandlerCanSendResponseThroughConnection)
    {
      WebSocketAdapter adapter{
          server_};

      auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      TransportHandlers handlers;

      handlers.onEnvelope =
          [](
              ConnectionPtr receivedConnection,
              const protocol::Envelope &envelope)
      {
        receivedConnection->send(
            envelope);
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const protocol::Envelope envelope =
          protocol::from_event(
              make_event());

      deliver_envelope(
          adapter,
          connection,
          envelope);

      ASSERT_EQ(
          connection->send_count(),
          1U);

      const protocol::Envelope &response =
          connection->sent_envelope(
              0);

      EXPECT_EQ(
          response.kind(),
          protocol::MessageKind::Event);

      EXPECT_EQ(
          response.type(),
          "counter.incremented");

      EXPECT_EQ(
          response.event_id(),
          envelope.event_id());
    }

    TEST_F(
        WebSocketAdapterProtocolTest,
        MultipleEnvelopesPreserveDeliveryOrder)
    {
      WebSocketAdapter adapter{
          server_};

      auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      std::vector<std::string>
          observedTypes;

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&](
              ConnectionPtr,
              const protocol::Envelope &envelope)
      {
        observedTypes.push_back(
            envelope.type());
      };

      adapter.set_handlers(
          std::move(
              handlers));

      RoomCommand first =
          make_command();

      RoomCommand second{
          make_room_id(),
          make_session_id(),
          "counter.decrement",
          JsonObject{},
          RequestId{
              "request-43"}};

      deliver_envelope(
          adapter,
          connection,
          protocol::from_command(
              first));

      deliver_envelope(
          adapter,
          connection,
          protocol::from_command(
              second));

      ASSERT_EQ(
          observedTypes.size(),
          2U);

      EXPECT_EQ(
          observedTypes[0],
          "counter.increment");

      EXPECT_EQ(
          observedTypes[1],
          "counter.decrement");
    }

    TEST_F(
        WebSocketAdapterProtocolTest,
        InvalidSerializedEnvelopeIsRejectedBeforeDelivery)
    {
      WebSocketAdapter adapter{
          server_};

      std::size_t callCount = 0;

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&](
              ConnectionPtr,
              const protocol::Envelope &)
      {
        ++callCount;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      EXPECT_THROW(
          static_cast<void>(
              protocol::parse(
                  "{invalid-json")),
          Error);

      EXPECT_EQ(
          callCount,
          0U);
    }

    TEST_F(
        WebSocketAdapterProtocolTest,
        AttachDoesNotModifyProtocolHandlers)
    {
      WebSocketAdapter adapter{
          server_};

      TransportHandlers handlers;

      handlers.onEnvelope =
          [](
              ConnectionPtr,
              const protocol::Envelope &) {};

      adapter.set_handlers(
          std::move(
              handlers));

      adapter.attach();

      EXPECT_TRUE(
          static_cast<bool>(
              adapter.handlers()
                  .onEnvelope));

      adapter.detach();

      EXPECT_TRUE(
          static_cast<bool>(
              adapter.handlers()
                  .onEnvelope));
    }

  } // namespace

} // namespace vix::realtime
