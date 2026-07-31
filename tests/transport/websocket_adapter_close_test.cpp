/**
 *
 * @file websocket_adapter_close_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for connection closure through the Vix Realtime WebSocket adapter.
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

#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/websocket/server.hpp>

#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/session.hpp>
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
               ("vix-realtime-close-" +
                std::to_string(
                    identifier) +
                ".env");
      }

      std::filesystem::path path_{};
    };

    class EmptyState final : public RoomState
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
        return std::make_unique<EmptyState>(
            *this);
      }
    };

    class EmptyHandler final : public RoomHandler
    {
    public:
      [[nodiscard]] CommandResult handle_command(
          const RoomCommand &,
          const RoomState &,
          const RoomContext &) override
      {
        return CommandResult::ignored();
      }
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
          const protocol::Envelope &) override
      {
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

      [[nodiscard]] JsonObject
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
      ConnectionId identifier_{};
      bool open_{true};

      std::optional<ErrorCode>
          closeCode_{};

      std::string closeReason_{};
      std::size_t closeCount_{0};

      JsonObject metadata_{};
    };

    template <typename RoomType>
    [[nodiscard]] bool join_session(
        RoomType &room,
        const std::shared_ptr<Session> &session)
    {
      if constexpr (
          requires {
            {
              room.join(session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.join(
            session);
      }
      else if constexpr (
          requires {
            room.join(session);
          })
      {
        room.join(
            session);

        return true;
      }
      else if constexpr (
          requires {
            {
              room.join(*session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.join(
            *session);
      }
      else if constexpr (
          requires {
            room.join(*session);
          })
      {
        room.join(
            *session);

        return true;
      }
      else if constexpr (
          requires {
            {
              room.join_session(session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.join_session(
            session);
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room join API");
      }
    }

    template <typename RoomType>
    [[nodiscard]] bool leave_session(
        RoomType &room,
        const SessionId &sessionId)
    {
      if constexpr (
          requires {
            {
              room.leave(sessionId)
            } -> std::convertible_to<bool>;
          })
      {
        return room.leave(
            sessionId);
      }
      else if constexpr (
          requires {
            room.leave(sessionId);
          })
      {
        room.leave(
            sessionId);

        return true;
      }
      else if constexpr (
          requires {
            {
              room.leave_session(
                  sessionId)
            } -> std::convertible_to<bool>;
          })
      {
        return room.leave_session(
            sessionId);
      }
      else if constexpr (
          requires {
            {
              room.remove_session(
                  sessionId)
            } -> std::convertible_to<bool>;
          })
      {
        return room.remove_session(
            sessionId);
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room leave API");
      }
    }

    class WebSocketAdapterCloseTest
        : public ::testing::Test
    {
    protected:
      WebSocketAdapterCloseTest()
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

    struct SessionFixture
    {
      std::shared_ptr<Session>
          session{};

      std::shared_ptr<RecordingConnection>
          connection{};
    };

    [[nodiscard]] RoomId make_room_id()
    {
      return RoomId{
          std::string_view{
              "city/river"}};
    }

    [[nodiscard]] SessionFixture make_session(
        std::string_view sessionId,
        std::string connectionId)
    {
      SessionFixture fixture;

      fixture.session =
          std::make_shared<Session>(
              SessionId{
                  sessionId});

      fixture.connection =
          std::make_shared<
              RecordingConnection>(
              std::move(
                  connectionId));

      fixture.session->attach(
          fixture.connection);

      return fixture;
    }

    [[nodiscard]] std::shared_ptr<Room>
    make_room()
    {
      auto room =
          std::make_shared<Room>(
              make_room_id(),
              std::make_unique<EmptyState>(),
              std::make_unique<EmptyHandler>(),
              std::make_shared<
                  MemoryEventStore>(),
              std::make_shared<
                  MemorySnapshotStore>(),
              Config{});

      room->open();

      return room;
    }

    void notify_close(
        WebSocketAdapter &adapter,
        const ConnectionPtr &connection)
    {
      ASSERT_TRUE(
          static_cast<bool>(
              adapter.handlers()
                  .onClose));

      adapter.handlers()
          .onClose(
              connection);
    }

    TEST_F(
        WebSocketAdapterCloseTest,
        DeliversClosedConnectionToHandler)
    {
      WebSocketAdapter adapter{
          server_};

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      ConnectionPtr observedConnection{};
      std::size_t closeCallbackCount = 0;

      TransportHandlers handlers;

      handlers.onClose =
          [&](
              ConnectionPtr closedConnection)
      {
        ++closeCallbackCount;

        observedConnection =
            std::move(
                closedConnection);
      };

      adapter.set_handlers(
          std::move(
              handlers));

      connection->close(
          ErrorCode::Cancelled,
          "client disconnected");

      notify_close(
          adapter,
          connection);

      EXPECT_EQ(
          closeCallbackCount,
          1U);

      EXPECT_EQ(
          observedConnection,
          connection);

      EXPECT_FALSE(
          connection->is_open());
    }

    TEST_F(
        WebSocketAdapterCloseTest,
        PreservesCloseReasonOnConnection)
    {
      WebSocketAdapter adapter{
          server_};

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      TransportHandlers handlers;

      handlers.onClose =
          [](
              ConnectionPtr) {};

      adapter.set_handlers(
          std::move(
              handlers));

      connection->close(
          ErrorCode::TransportFailure,
          "socket closed unexpectedly");

      notify_close(
          adapter,
          connection);

      ASSERT_TRUE(
          connection->close_code()
              .has_value());

      EXPECT_EQ(
          *connection->close_code(),
          ErrorCode::TransportFailure);

      EXPECT_EQ(
          connection->close_reason(),
          "socket closed unexpectedly");

      EXPECT_EQ(
          connection->close_count(),
          1U);
    }

    TEST_F(
        WebSocketAdapterCloseTest,
        RemovesSessionFromRoomOnClose)
    {
      WebSocketAdapter adapter{
          server_};

      const auto room =
          make_room();

      SessionFixture fixture =
          make_session(
              "session-42",
              "connection-42");

      ASSERT_TRUE(
          join_session(
              *room,
              fixture.session));

      ASSERT_EQ(
          room->member_count(),
          1U);

      TransportHandlers handlers;

      handlers.onClose =
          [room,
           session = fixture.session](
              ConnectionPtr)
      {
        static_cast<void>(
            leave_session(
                *room,
                session->id()));

        session->detach();
      };

      adapter.set_handlers(
          std::move(
              handlers));

      fixture.connection->close(
          ErrorCode::Cancelled,
          "client disconnected");

      notify_close(
          adapter,
          fixture.connection);

      EXPECT_EQ(
          room->member_count(),
          0U);

      EXPECT_TRUE(
          room->empty());

      EXPECT_FALSE(
          fixture.session->has_room(
              room->id()));

      EXPECT_EQ(
          fixture.session->room_count(),
          0U);
    }

    TEST_F(
        WebSocketAdapterCloseTest,
        ClosingOneConnectionKeepsOtherSession)
    {
      WebSocketAdapter adapter{
          server_};

      const auto room =
          make_room();

      SessionFixture first =
          make_session(
              "session-1",
              "connection-1");

      SessionFixture second =
          make_session(
              "session-2",
              "connection-2");

      ASSERT_TRUE(
          join_session(
              *room,
              first.session));

      ASSERT_TRUE(
          join_session(
              *room,
              second.session));

      TransportHandlers handlers;

      handlers.onClose =
          [room,
           first,
           second](
              ConnectionPtr connection)
      {
        if (connection ==
            first.connection)
        {
          static_cast<void>(
              leave_session(
                  *room,
                  first.session->id()));

          first.session->detach();
        }
        else if (connection ==
                 second.connection)
        {
          static_cast<void>(
              leave_session(
                  *room,
                  second.session->id()));

          second.session->detach();
        }
      };

      adapter.set_handlers(
          std::move(
              handlers));

      first.connection->close(
          ErrorCode::Cancelled,
          "first disconnected");

      notify_close(
          adapter,
          first.connection);

      EXPECT_EQ(
          room->member_count(),
          1U);

      EXPECT_FALSE(
          first.session->has_room(
              room->id()));

      EXPECT_TRUE(
          second.session->has_room(
              room->id()));

      EXPECT_TRUE(
          second.connection->is_open());
    }

    TEST_F(
        WebSocketAdapterCloseTest,
        DuplicateCloseNotificationIsIdempotent)
    {
      WebSocketAdapter adapter{
          server_};

      const auto room =
          make_room();

      SessionFixture fixture =
          make_session(
              "session-42",
              "connection-42");

      ASSERT_TRUE(
          join_session(
              *room,
              fixture.session));

      std::size_t callbackCount = 0;

      TransportHandlers handlers;

      handlers.onClose =
          [room,
           session = fixture.session,
           &callbackCount](
              ConnectionPtr)
      {
        ++callbackCount;

        static_cast<void>(
            leave_session(
                *room,
                session->id()));

        session->detach();
      };

      adapter.set_handlers(
          std::move(
              handlers));

      fixture.connection->close(
          ErrorCode::Cancelled,
          "client disconnected");

      notify_close(
          adapter,
          fixture.connection);

      notify_close(
          adapter,
          fixture.connection);

      EXPECT_EQ(
          callbackCount,
          2U);

      EXPECT_EQ(
          room->member_count(),
          0U);

      EXPECT_EQ(
          fixture.session->room_count(),
          0U);
    }

    TEST_F(
        WebSocketAdapterCloseTest,
        CloseHandlerSurvivesAttachAndDetach)
    {
      WebSocketAdapter adapter{
          server_};

      std::size_t callbackCount = 0;

      TransportHandlers handlers;

      handlers.onClose =
          [&callbackCount](
              ConnectionPtr)
      {
        ++callbackCount;
      };

      adapter.set_handlers(
          std::move(
              handlers));

      adapter.attach();
      adapter.detach();

      ASSERT_TRUE(
          static_cast<bool>(
              adapter.handlers()
                  .onClose));

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      connection->close(
          ErrorCode::Cancelled,
          "closed after detach");

      notify_close(
          adapter,
          connection);

      EXPECT_EQ(
          callbackCount,
          1U);

      EXPECT_FALSE(
          connection->is_open());
    }

    TEST_F(
        WebSocketAdapterCloseTest,
        CloseDoesNotCloseRoomItself)
    {
      WebSocketAdapter adapter{
          server_};

      const auto room =
          make_room();

      SessionFixture fixture =
          make_session(
              "session-42",
              "connection-42");

      ASSERT_TRUE(
          join_session(
              *room,
              fixture.session));

      TransportHandlers handlers;

      handlers.onClose =
          [room,
           session = fixture.session](
              ConnectionPtr)
      {
        static_cast<void>(
            leave_session(
                *room,
                session->id()));

        session->detach();
      };

      adapter.set_handlers(
          std::move(
              handlers));

      fixture.connection->close(
          ErrorCode::Cancelled,
          "client disconnected");

      notify_close(
          adapter,
          fixture.connection);

      EXPECT_TRUE(
          room->is_open());

      EXPECT_FALSE(
          room->is_closed());

      EXPECT_EQ(
          room->status(),
          RoomStatus::Open);
    }

  } // namespace

} // namespace vix::realtime
