/**
 *
 * @file websocket_adapter_join_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for joining rooms through the Vix Realtime WebSocket adapter.
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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/websocket/server.hpp>

#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
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
               ("vix-realtime-join-" +
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
      else if constexpr (
          requires {
            room.join_session(session);
          })
      {
        room.join_session(
            session);

        return true;
      }
      else if constexpr (
          requires {
            {
              room.add_session(session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.add_session(
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
    [[nodiscard]] bool has_session(
        const RoomType &room,
        const SessionId &sessionId)
    {
      if constexpr (
          requires {
            {
              room.has_member(sessionId)
            } -> std::convertible_to<bool>;
          })
      {
        return room.has_member(
            sessionId);
      }
      else if constexpr (
          requires {
            {
              room.has_session(sessionId)
            } -> std::convertible_to<bool>;
          })
      {
        return room.has_session(
            sessionId);
      }
      else if constexpr (
          requires {
            {
              room.contains_session(
                  sessionId)
            } -> std::convertible_to<bool>;
          })
      {
        return room.contains_session(
            sessionId);
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room membership API");
      }
    }

    class WebSocketAdapterJoinTest
        : public ::testing::Test
    {
    protected:
      WebSocketAdapterJoinTest()
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
              "city/river"}};
    }

    [[nodiscard]] RoomId make_other_room_id()
    {
      return RoomId{
          std::string_view{
              "city/forest"}};
    }

    [[nodiscard]] SessionId make_session_id(
        std::string_view value =
            "session-42")
    {
      return SessionId{
          value};
    }

    [[nodiscard]] std::shared_ptr<Room>
    make_room(
        RoomId roomId =
            make_room_id())
    {
      auto room =
          std::make_shared<Room>(
              std::move(
                  roomId),
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

    [[nodiscard]] RoomCommand make_join_command(
        RoomId roomId =
            make_room_id(),
        SessionId sessionId =
            make_session_id())
    {
      return RoomCommand{
          std::move(
              roomId),
          std::move(
              sessionId),
          "room.join",
          JsonObject{},
          RequestId{
              "request-42"}};
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
        WebSocketAdapterJoinTest,
        JoinEnvelopeAddsSessionToRoom)
    {
      WebSocketAdapter adapter{
          server_};

      const auto room =
          make_room();

      const auto session =
          std::make_shared<Session>(
              make_session_id());

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      session->attach(
          connection);

      TransportHandlers handlers;

      handlers.onEnvelope =
          [room,
           session](
              ConnectionPtr,
              const protocol::Envelope &envelope)
      {
        if (envelope.type() !=
            "room.join")
        {
          return;
        }

        if (!envelope.room_id()
                 .has_value())
        {
          return;
        }

        if (*envelope.room_id() !=
            room->id())
        {
          return;
        }

        static_cast<void>(
            join_session(
                *room,
                session));
      };

      adapter.set_handlers(
          std::move(
              handlers));

      adapter.attach();

      deliver_envelope(
          adapter,
          connection,
          protocol::from_command(
              make_join_command()));

      EXPECT_EQ(
          room->member_count(),
          1U);

      EXPECT_TRUE(
          has_session(
              *room,
              session->id()));

      EXPECT_TRUE(
          session->has_room(
              room->id()));
    }

    TEST_F(
        WebSocketAdapterJoinTest,
        DuplicateJoinIsIdempotent)
    {
      WebSocketAdapter adapter{
          server_};

      const auto room =
          make_room();

      const auto session =
          std::make_shared<Session>(
              make_session_id());

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      session->attach(
          connection);

      TransportHandlers handlers;

      handlers.onEnvelope =
          [room,
           session](
              ConnectionPtr,
              const protocol::Envelope &envelope)
      {
        if (envelope.type() ==
                "room.join" &&
            envelope.room_id()
                .has_value() &&
            *envelope.room_id() ==
                room->id())
        {
          static_cast<void>(
              join_session(
                  *room,
                  session));
        }
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const protocol::Envelope joinEnvelope =
          protocol::from_command(
              make_join_command());

      deliver_envelope(
          adapter,
          connection,
          joinEnvelope);

      deliver_envelope(
          adapter,
          connection,
          joinEnvelope);

      EXPECT_EQ(
          room->member_count(),
          1U);

      EXPECT_EQ(
          session->room_count(),
          1U);
    }

    TEST_F(
        WebSocketAdapterJoinTest,
        JoinForDifferentRoomIsIgnored)
    {
      WebSocketAdapter adapter{
          server_};

      const auto room =
          make_room();

      const auto session =
          std::make_shared<Session>(
              make_session_id());

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      session->attach(
          connection);

      TransportHandlers handlers;

      handlers.onEnvelope =
          [room,
           session](
              ConnectionPtr,
              const protocol::Envelope &envelope)
      {
        if (envelope.type() !=
                "room.join" ||
            !envelope.room_id()
                 .has_value() ||
            *envelope.room_id() !=
                room->id())
        {
          return;
        }

        static_cast<void>(
            join_session(
                *room,
                session));
      };

      adapter.set_handlers(
          std::move(
              handlers));

      deliver_envelope(
          adapter,
          connection,
          protocol::from_command(
              make_join_command(
                  make_other_room_id())));

      EXPECT_EQ(
          room->member_count(),
          0U);

      EXPECT_FALSE(
          session->has_room(
              room->id()));
    }

    TEST_F(
        WebSocketAdapterJoinTest,
        NonJoinCommandDoesNotChangeMembership)
    {
      WebSocketAdapter adapter{
          server_};

      const auto room =
          make_room();

      const auto session =
          std::make_shared<Session>(
              make_session_id());

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      session->attach(
          connection);

      TransportHandlers handlers;

      handlers.onEnvelope =
          [room,
           session](
              ConnectionPtr,
              const protocol::Envelope &envelope)
      {
        if (envelope.type() ==
            "room.join")
        {
          static_cast<void>(
              join_session(
                  *room,
                  session));
        }
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const RoomCommand command{
          make_room_id(),
          make_session_id(),
          "room.command",
          JsonObject{},
          RequestId{
              "request-42"}};

      deliver_envelope(
          adapter,
          connection,
          protocol::from_command(
              command));

      EXPECT_EQ(
          room->member_count(),
          0U);

      EXPECT_FALSE(
          session->has_room(
              room->id()));
    }

    TEST_F(
        WebSocketAdapterJoinTest,
        JoinedSessionCanReceiveRoomEnvelope)
    {
      WebSocketAdapter adapter{
          server_};

      const auto room =
          make_room();

      const auto session =
          std::make_shared<Session>(
              make_session_id());

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      session->attach(
          connection);

      TransportHandlers handlers;

      handlers.onEnvelope =
          [room,
           session](
              ConnectionPtr receivedConnection,
              const protocol::Envelope &envelope)
      {
        if (envelope.type() !=
                "room.join" ||
            !envelope.room_id()
                 .has_value() ||
            *envelope.room_id() !=
                room->id())
        {
          return;
        }

        static_cast<void>(
            join_session(
                *room,
                session));

        receivedConnection->send(
            envelope);
      };

      adapter.set_handlers(
          std::move(
              handlers));

      const protocol::Envelope joinEnvelope =
          protocol::from_command(
              make_join_command());

      deliver_envelope(
          adapter,
          connection,
          joinEnvelope);

      ASSERT_TRUE(
          has_session(
              *room,
              session->id()));

      ASSERT_EQ(
          connection->send_count(),
          1U);

      EXPECT_EQ(
          connection->sent_envelope(
                        0)
              .type(),
          "room.join");

      EXPECT_EQ(
          connection->sent_envelope(
                        0)
              .room_id(),
          joinEnvelope.room_id());
    }

    TEST_F(
        WebSocketAdapterJoinTest,
        MultipleSessionsCanJoinSameRoom)
    {
      WebSocketAdapter adapter{
          server_};

      const auto room =
          make_room();

      const auto firstSession =
          std::make_shared<Session>(
              make_session_id(
                  "session-1"));

      const auto secondSession =
          std::make_shared<Session>(
              make_session_id(
                  "session-2"));

      const auto firstConnection =
          std::make_shared<
              RecordingConnection>(
              "connection-1");

      const auto secondConnection =
          std::make_shared<
              RecordingConnection>(
              "connection-2");

      firstSession->attach(
          firstConnection);

      secondSession->attach(
          secondConnection);

      TransportHandlers handlers;

      handlers.onEnvelope =
          [room,
           firstSession,
           secondSession,
           firstConnection,
           secondConnection](
              ConnectionPtr connection,
              const protocol::Envelope &envelope)
      {
        if (envelope.type() !=
                "room.join" ||
            !envelope.room_id()
                 .has_value() ||
            *envelope.room_id() !=
                room->id())
        {
          return;
        }

        if (connection ==
            firstConnection)
        {
          static_cast<void>(
              join_session(
                  *room,
                  firstSession));
        }
        else if (connection ==
                 secondConnection)
        {
          static_cast<void>(
              join_session(
                  *room,
                  secondSession));
        }
      };

      adapter.set_handlers(
          std::move(
              handlers));

      deliver_envelope(
          adapter,
          firstConnection,
          protocol::from_command(
              make_join_command(
                  make_room_id(),
                  firstSession->id())));

      deliver_envelope(
          adapter,
          secondConnection,
          protocol::from_command(
              make_join_command(
                  make_room_id(),
                  secondSession->id())));

      EXPECT_EQ(
          room->member_count(),
          2U);

      EXPECT_TRUE(
          has_session(
              *room,
              firstSession->id()));

      EXPECT_TRUE(
          has_session(
              *room,
              secondSession->id()));
    }

    TEST_F(
        WebSocketAdapterJoinTest,
        JoinHandlerRemainsAvailableAfterAttach)
    {
      WebSocketAdapter adapter{
          server_};

      const auto room =
          make_room();

      const auto session =
          std::make_shared<Session>(
              make_session_id());

      TransportHandlers handlers;

      handlers.onEnvelope =
          [room,
           session](
              ConnectionPtr,
              const protocol::Envelope &envelope)
      {
        if (envelope.type() ==
            "room.join")
        {
          static_cast<void>(
              join_session(
                  *room,
                  session));
        }
      };

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
