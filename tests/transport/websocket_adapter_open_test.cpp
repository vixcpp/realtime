/**
 *
 * @file websocket_adapter_open_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for attaching the Vix Realtime WebSocket adapter.
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
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/websocket/server.hpp>

#include <vix/realtime/connection.hpp>
#include <vix/realtime/transport.hpp>
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

        output.close();
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
               ("vix-realtime-websocket-" +
                std::to_string(
                    identifier) +
                ".env");
      }

      std::filesystem::path path_{};
    };

    class WebSocketAdapterOpenTest
        : public ::testing::Test
    {
    protected:
      WebSocketAdapterOpenTest()
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

    TEST_F(WebSocketAdapterOpenTest, StartsDetached)
    {
      WebSocketAdapter adapter{
          server_};

      EXPECT_FALSE(
          adapter.attached());

      EXPECT_EQ(
          adapter.connection_count(),
          0U);

      EXPECT_TRUE(
          adapter.connections()
              .empty());
    }

    TEST_F(WebSocketAdapterOpenTest, AttachesToWebSocketServer)
    {
      WebSocketAdapter adapter{
          server_};

      adapter.attach();

      EXPECT_TRUE(
          adapter.attached());

      EXPECT_EQ(
          adapter.connection_count(),
          0U);
    }

    TEST_F(WebSocketAdapterOpenTest, AttachIsIdempotent)
    {
      WebSocketAdapter adapter{
          server_};

      adapter.attach();

      EXPECT_NO_THROW(
          adapter.attach());

      EXPECT_TRUE(
          adapter.attached());

      EXPECT_EQ(
          adapter.connection_count(),
          0U);
    }

    TEST_F(WebSocketAdapterOpenTest, ExposesUnderlyingServer)
    {
      WebSocketAdapter adapter{
          server_};

      EXPECT_EQ(
          std::addressof(
              adapter.websocket_server()),
          std::addressof(
              server_));
    }

    TEST_F(WebSocketAdapterOpenTest, StoresCustomOptions)
    {
      WebSocketAdapterOptions options;

      options.maxMessageSize =
          4096;

      options.closeOnProtocolError =
          false;

      options.connectionIdPrefix =
          "test-connection-";

      WebSocketAdapter adapter{
          server_,
          options};

      EXPECT_EQ(
          adapter.options()
              .maxMessageSize,
          4096U);

      EXPECT_FALSE(
          adapter.options()
              .closeOnProtocolError);

      EXPECT_EQ(
          adapter.options()
              .connectionIdPrefix,
          "test-connection-");
    }

    TEST_F(WebSocketAdapterOpenTest, StoresTransportHandlers)
    {
      WebSocketAdapter adapter{
          server_};

      bool opened = false;

      TransportHandlers handlers;

      handlers.onOpen =
          [&opened](
              ConnectionPtr connection)
      {
        opened =
            connection != nullptr;
      };

      adapter.set_handlers(
          std::move(handlers));

      EXPECT_TRUE(
          static_cast<bool>(
              adapter.handlers()
                  .onOpen));

      EXPECT_FALSE(
          opened);
    }

    TEST_F(WebSocketAdapterOpenTest, RetainsHandlersAfterAttach)
    {
      WebSocketAdapter adapter{
          server_};

      TransportHandlers handlers;

      handlers.onOpen =
          [](
              ConnectionPtr) {};

      handlers.onEnvelope =
          [](
              ConnectionPtr,
              const protocol::Envelope &) {};

      handlers.onClose =
          [](
              ConnectionPtr) {};

      handlers.onError =
          [](
              ConnectionPtr,
              ErrorCode,
              std::string_view) {};

      adapter.set_handlers(
          std::move(handlers));

      adapter.attach();

      EXPECT_TRUE(
          static_cast<bool>(
              adapter.handlers()
                  .onOpen));

      EXPECT_TRUE(
          static_cast<bool>(
              adapter.handlers()
                  .onEnvelope));

      EXPECT_TRUE(
          static_cast<bool>(
              adapter.handlers()
                  .onClose));

      EXPECT_TRUE(
          static_cast<bool>(
              adapter.handlers()
                  .onError));
    }

    TEST_F(WebSocketAdapterOpenTest, CannotFindConnectionBeforeOpen)
    {
      WebSocketAdapter adapter{
          server_};

      adapter.attach();

      EXPECT_EQ(
          adapter.find_connection(
              "missing-connection"),
          nullptr);

      EXPECT_EQ(
          adapter.connection_count(),
          0U);
    }

    TEST_F(WebSocketAdapterOpenTest, DetachesFromWebSocketServer)
    {
      WebSocketAdapter adapter{
          server_};

      adapter.attach();

      ASSERT_TRUE(
          adapter.attached());

      adapter.detach();

      EXPECT_FALSE(
          adapter.attached());

      EXPECT_EQ(
          adapter.connection_count(),
          0U);

      EXPECT_TRUE(
          adapter.connections()
              .empty());
    }

    TEST_F(WebSocketAdapterOpenTest, DetachIsIdempotent)
    {
      WebSocketAdapter adapter{
          server_};

      EXPECT_NO_THROW(
          adapter.detach());

      adapter.attach();
      adapter.detach();

      EXPECT_NO_THROW(
          adapter.detach());

      EXPECT_FALSE(
          adapter.attached());
    }

    TEST_F(WebSocketAdapterOpenTest, CanAttachAgainAfterDetach)
    {
      WebSocketAdapter adapter{
          server_};

      adapter.attach();
      adapter.detach();

      ASSERT_FALSE(
          adapter.attached());

      adapter.attach();

      EXPECT_TRUE(
          adapter.attached());

      EXPECT_EQ(
          adapter.connection_count(),
          0U);
    }

  } // namespace

} // namespace vix::realtime
