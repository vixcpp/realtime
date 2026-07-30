/**
 *
 * @file websocket_adapter_backpressure_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for inbound backpressure in the Vix Realtime WebSocket adapter.
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
#include <vix/websocket/server.hpp>

#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_id.hpp>
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
               ("vix-realtime-backpressure-" +
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
          const protocol::Envelope &) override
      {
        ++sendCount_;
      }

      void close(
          ErrorCode code,
          std::string_view reason) override
      {
        open_ = false;
        closeCode_ = code;
        closeReason_ = reason;
        ++closeCount_;
      }

      [[nodiscard]] const JsonObject &
      metadata() const noexcept override
      {
        return metadata_;
      }

      [[nodiscard]] const std::optional<ErrorCode> &
      close_code() const noexcept
      {
        return closeCode_;
      }

      [[nodiscard]] const std::string &
      close_reason() const noexcept
      {
        return closeReason_;
      }

      [[nodiscard]] std::size_t
      close_count() const noexcept
      {
        return closeCount_;
      }

    private:
      std::string identifier_{};
      bool open_{true};

      std::size_t sendCount_{0};
      std::size_t closeCount_{0};

      std::optional<ErrorCode>
          closeCode_{};

      std::string closeReason_{};
      JsonObject metadata_{};
    };

    enum class DeliveryResult
    {
      Delivered,
      MessageTooLarge,
      InvalidProtocol
    };

    class WebSocketAdapterBackpressureTest
        : public ::testing::Test
    {
    protected:
      WebSocketAdapterBackpressureTest()
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

    [[nodiscard]] RoomCommand make_command(
        std::string requestId =
            "request-42")
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          5);

      return RoomCommand{
          RoomId{
              std::string_view{
                  "counter/main"}},
          SessionId{
              std::string_view{
                  "session-42"}},
          "room.command",
          std::move(payload),
          std::move(requestId)};
    }

    [[nodiscard]] std::string
    serialized_command(
        std::string requestId =
            "request-42")
    {
      return protocol::serialize(
          protocol::from_command(
              make_command(
                  std::move(
                      requestId))));
    }

    void report_protocol_error(
        WebSocketAdapter &adapter,
        const ConnectionPtr &connection,
        std::string_view reason)
    {
      if (adapter.handlers()
              .onError)
      {
        adapter.handlers()
            .onError(
                connection,
                ErrorCode::TransportFailure,
                reason);
      }

      if (adapter.options()
              .closeOnProtocolError)
      {
        connection->close(
            ErrorCode::TransportFailure,
            reason);
      }
    }

    [[nodiscard]] DeliveryResult
    deliver_text(
        WebSocketAdapter &adapter,
        const ConnectionPtr &connection,
        std::string_view text)
    {
      if (text.size() >
          adapter.options()
              .maxMessageSize)
      {
        report_protocol_error(
            adapter,
            connection,
            "WebSocket message exceeds maximum size");

        return DeliveryResult::MessageTooLarge;
      }

      try
      {
        const protocol::Envelope envelope =
            protocol::parse(
                text);

        if (adapter.handlers()
                .onEnvelope)
        {
          adapter.handlers()
              .onEnvelope(
                  connection,
                  envelope);
        }

        return DeliveryResult::Delivered;
      }
      catch (const Error &)
      {
        report_protocol_error(
            adapter,
            connection,
            "invalid WebSocket protocol envelope");

        return DeliveryResult::InvalidProtocol;
      }
    }

    TEST_F(
        WebSocketAdapterBackpressureTest,
        DeliversMessageBelowConfiguredLimit)
    {
      const std::string message =
          serialized_command();

      WebSocketAdapterOptions options;

      options.maxMessageSize =
          message.size() + 1U;

      options.closeOnProtocolError =
          true;

      WebSocketAdapter adapter{
          server_,
          options};

      std::size_t deliveredCount = 0;

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&deliveredCount](
              ConnectionPtr,
              const protocol::Envelope &)
      {
        ++deliveredCount;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      EXPECT_EQ(
          deliver_text(
              adapter,
              connection,
              message),
          DeliveryResult::Delivered);

      EXPECT_EQ(
          deliveredCount,
          1U);

      EXPECT_TRUE(
          connection->is_open());
    }

    TEST_F(
        WebSocketAdapterBackpressureTest,
        DeliversMessageExactlyAtConfiguredLimit)
    {
      const std::string message =
          serialized_command();

      WebSocketAdapterOptions options;

      options.maxMessageSize =
          message.size();

      options.closeOnProtocolError =
          true;

      WebSocketAdapter adapter{
          server_,
          options};

      std::size_t deliveredCount = 0;

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&deliveredCount](
              ConnectionPtr,
              const protocol::Envelope &)
      {
        ++deliveredCount;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      EXPECT_EQ(
          deliver_text(
              adapter,
              connection,
              message),
          DeliveryResult::Delivered);

      EXPECT_EQ(
          deliveredCount,
          1U);

      EXPECT_TRUE(
          connection->is_open());
    }

    TEST_F(
        WebSocketAdapterBackpressureTest,
        RejectsMessageAboveConfiguredLimit)
    {
      const std::string message =
          serialized_command();

      ASSERT_GT(
          message.size(),
          1U);

      WebSocketAdapterOptions options;

      options.maxMessageSize =
          message.size() - 1U;

      options.closeOnProtocolError =
          false;

      WebSocketAdapter adapter{
          server_,
          options};

      std::size_t deliveredCount = 0;

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&deliveredCount](
              ConnectionPtr,
              const protocol::Envelope &)
      {
        ++deliveredCount;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      EXPECT_EQ(
          deliver_text(
              adapter,
              connection,
              message),
          DeliveryResult::MessageTooLarge);

      EXPECT_EQ(
          deliveredCount,
          0U);

      EXPECT_TRUE(
          connection->is_open());
    }

    TEST_F(
        WebSocketAdapterBackpressureTest,
        ReportsOversizedMessageToErrorHandler)
    {
      const std::string message =
          serialized_command();

      WebSocketAdapterOptions options;

      options.maxMessageSize =
          1U;

      options.closeOnProtocolError =
          false;

      WebSocketAdapter adapter{
          server_,
          options};

      ConnectionPtr observedConnection{};
      std::optional<ErrorCode>
          observedCode{};

      std::string observedReason{};

      TransportHandlers handlers;

      handlers.onError =
          [&](
              ConnectionPtr connection,
              ErrorCode code,
              std::string_view reason)
      {
        observedConnection =
            std::move(
                connection);

        observedCode =
            code;

        observedReason =
            reason;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      EXPECT_EQ(
          deliver_text(
              adapter,
              connection,
              message),
          DeliveryResult::MessageTooLarge);

      EXPECT_EQ(
          observedConnection,
          connection);

      ASSERT_TRUE(
          observedCode
              .has_value());

      EXPECT_EQ(
          *observedCode,
          ErrorCode::TransportFailure);

      EXPECT_FALSE(
          observedReason.empty());
    }

    TEST_F(
        WebSocketAdapterBackpressureTest,
        ClosesConnectionWhenConfigured)
    {
      const std::string message =
          serialized_command();

      WebSocketAdapterOptions options;

      options.maxMessageSize =
          1U;

      options.closeOnProtocolError =
          true;

      WebSocketAdapter adapter{
          server_,
          options};

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      EXPECT_EQ(
          deliver_text(
              adapter,
              connection,
              message),
          DeliveryResult::MessageTooLarge);

      EXPECT_FALSE(
          connection->is_open());

      EXPECT_EQ(
          connection->close_count(),
          1U);

      ASSERT_TRUE(
          connection->close_code()
              .has_value());

      EXPECT_EQ(
          *connection->close_code(),
          ErrorCode::TransportFailure);

      EXPECT_FALSE(
          connection->close_reason()
              .empty());
    }

    TEST_F(
        WebSocketAdapterBackpressureTest,
        KeepsConnectionOpenWhenClosingIsDisabled)
    {
      const std::string message =
          serialized_command();

      WebSocketAdapterOptions options;

      options.maxMessageSize =
          1U;

      options.closeOnProtocolError =
          false;

      WebSocketAdapter adapter{
          server_,
          options};

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      EXPECT_EQ(
          deliver_text(
              adapter,
              connection,
              message),
          DeliveryResult::MessageTooLarge);

      EXPECT_TRUE(
          connection->is_open());

      EXPECT_EQ(
          connection->close_count(),
          0U);

      EXPECT_FALSE(
          connection->close_code()
              .has_value());
    }

    TEST_F(
        WebSocketAdapterBackpressureTest,
        CanProcessLaterValidMessageWhenConnectionRemainsOpen)
    {
      const std::string validMessage =
          serialized_command();

      WebSocketAdapterOptions options;

      options.maxMessageSize =
          validMessage.size();

      options.closeOnProtocolError =
          false;

      WebSocketAdapter adapter{
          server_,
          options};

      std::size_t deliveredCount = 0;
      std::size_t errorCount = 0;

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&deliveredCount](
              ConnectionPtr,
              const protocol::Envelope &)
      {
        ++deliveredCount;
      };

      handlers.onError =
          [&errorCount](
              ConnectionPtr,
              ErrorCode,
              std::string_view)
      {
        ++errorCount;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      const std::string oversizedMessage(
          validMessage.size() + 1U,
          'x');

      EXPECT_EQ(
          deliver_text(
              adapter,
              connection,
              oversizedMessage),
          DeliveryResult::MessageTooLarge);

      ASSERT_TRUE(
          connection->is_open());

      EXPECT_EQ(
          deliver_text(
              adapter,
              connection,
              validMessage),
          DeliveryResult::Delivered);

      EXPECT_EQ(
          errorCount,
          1U);

      EXPECT_EQ(
          deliveredCount,
          1U);
    }

    TEST_F(
        WebSocketAdapterBackpressureTest,
        InvalidProtocolDoesNotReachEnvelopeHandler)
    {
      WebSocketAdapterOptions options;

      options.maxMessageSize =
          1024U;

      options.closeOnProtocolError =
          false;

      WebSocketAdapter adapter{
          server_,
          options};

      std::size_t deliveredCount = 0;
      std::size_t errorCount = 0;

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&deliveredCount](
              ConnectionPtr,
              const protocol::Envelope &)
      {
        ++deliveredCount;
      };

      handlers.onError =
          [&errorCount](
              ConnectionPtr,
              ErrorCode,
              std::string_view)
      {
        ++errorCount;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      EXPECT_EQ(
          deliver_text(
              adapter,
              connection,
              "{invalid-json"),
          DeliveryResult::InvalidProtocol);

      EXPECT_EQ(
          deliveredCount,
          0U);

      EXPECT_EQ(
          errorCount,
          1U);

      EXPECT_TRUE(
          connection->is_open());
    }

    TEST_F(
        WebSocketAdapterBackpressureTest,
        BackpressureIsIsolatedPerConnection)
    {
      const std::string validMessage =
          serialized_command();

      WebSocketAdapterOptions options;

      options.maxMessageSize =
          validMessage.size();

      options.closeOnProtocolError =
          true;

      WebSocketAdapter adapter{
          server_,
          options};

      std::size_t deliveredCount = 0;

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&deliveredCount](
              ConnectionPtr,
              const protocol::Envelope &)
      {
        ++deliveredCount;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const auto overloaded =
          std::make_shared<
              RecordingConnection>(
              "connection-overloaded");

      const auto healthy =
          std::make_shared<
              RecordingConnection>(
              "connection-healthy");

      const std::string oversizedMessage(
          validMessage.size() + 1U,
          'x');

      EXPECT_EQ(
          deliver_text(
              adapter,
              overloaded,
              oversizedMessage),
          DeliveryResult::MessageTooLarge);

      EXPECT_FALSE(
          overloaded->is_open());

      EXPECT_EQ(
          deliver_text(
              adapter,
              healthy,
              validMessage),
          DeliveryResult::Delivered);

      EXPECT_TRUE(
          healthy->is_open());

      EXPECT_EQ(
          deliveredCount,
          1U);
    }

    TEST_F(
        WebSocketAdapterBackpressureTest,
        ProcessesMultipleMessagesWithinLimit)
    {
      const std::string first =
          serialized_command(
              "request-1");

      const std::string second =
          serialized_command(
              "request-2");

      const std::size_t limit =
          first.size() >
                  second.size()
              ? first.size()
              : second.size();

      WebSocketAdapterOptions options;

      options.maxMessageSize =
          limit;

      options.closeOnProtocolError =
          true;

      WebSocketAdapter adapter{
          server_,
          options};

      std::vector<RequestId>
          requestIds;

      TransportHandlers handlers;

      handlers.onEnvelope =
          [&requestIds](
              ConnectionPtr,
              const protocol::Envelope &envelope)
      {
        requestIds.push_back(
            envelope.request_id());
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      EXPECT_EQ(
          deliver_text(
              adapter,
              connection,
              first),
          DeliveryResult::Delivered);

      EXPECT_EQ(
          deliver_text(
              adapter,
              connection,
              second),
          DeliveryResult::Delivered);

      ASSERT_EQ(
          requestIds.size(),
          2U);

      EXPECT_EQ(
          requestIds[0],
          "request-1");

      EXPECT_EQ(
          requestIds[1],
          "request-2");

      EXPECT_TRUE(
          connection->is_open());
    }

  } // namespace

} // namespace vix::realtime
