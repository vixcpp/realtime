/**
 *
 * @file room_broadcast_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for event broadcasting in Vix Realtime rooms.
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
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/json/json.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_id.hpp>
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
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/session.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    template <typename>
    inline constexpr bool dependentFalse = false;

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
          const protocol::Envelope &envelope) override
      {
        envelopes_.push_back(
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
        return envelopes_.size();
      }

      [[nodiscard]] const protocol::Envelope &
      last_envelope() const
      {
        return envelopes_.back();
      }

      void clear()
      {
        envelopes_.clear();
      }

    private:
      ConnectionId identifier_{};
      bool open_{true};
      ErrorCode closeCode_{ErrorCode::None};
      std::string closeReason_{};
      JsonObject metadata_{};
      std::vector<protocol::Envelope>
          envelopes_{};
    };

    template <typename RoomType>
    void join_session(
        RoomType &room,
        const std::shared_ptr<Session> &session)
    {
      if constexpr (
          requires {
            room.join(session);
          })
      {
        static_cast<void>(
            room.join(
                session));
      }
      else if constexpr (
          requires {
            room.join(*session);
          })
      {
        static_cast<void>(
            room.join(
                *session));
      }
      else if constexpr (
          requires {
            room.join_session(session);
          })
      {
        static_cast<void>(
            room.join_session(
                session));
      }
      else if constexpr (
          requires {
            room.add_session(session);
          })
      {
        static_cast<void>(
            room.add_session(
                session));
      }
      else if constexpr (
          requires {
            room.add_member(session);
          })
      {
        static_cast<void>(
            room.add_member(
                session));
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room session join API");
      }
    }

    template <typename RoomType>
    void leave_session(
        RoomType &room,
        const std::shared_ptr<Session> &session)
    {
      if constexpr (
          requires {
            room.leave(
                session->id());
          })
      {
        static_cast<void>(
            room.leave(
                session->id()));
      }
      else if constexpr (
          requires {
            room.leave(session);
          })
      {
        static_cast<void>(
            room.leave(
                session));
      }
      else if constexpr (
          requires {
            room.leave_session(
                session->id());
          })
      {
        static_cast<void>(
            room.leave_session(
                session->id()));
      }
      else if constexpr (
          requires {
            room.remove_session(
                session->id());
          })
      {
        static_cast<void>(
            room.remove_session(
                session->id()));
      }
      else if constexpr (
          requires {
            room.remove_member(
                session->id());
          })
      {
        static_cast<void>(
            room.remove_member(
                session->id()));
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room session leave API");
      }
    }

    template <typename RoomType>
    void broadcast_event(
        RoomType &room,
        const RoomEvent &event)
    {
      if constexpr (
          requires {
            room.broadcast(event);
          })
      {
        static_cast<void>(
            room.broadcast(
                event));
      }
      else if constexpr (
          requires {
            room.broadcast_event(event);
          })
      {
        static_cast<void>(
            room.broadcast_event(
                event));
      }
      else if constexpr (
          requires {
            room.publish_event(event);
          })
      {
        static_cast<void>(
            room.publish_event(
                event));
      }
      else if constexpr (
          requires {
            room.emit(event);
          })
      {
        static_cast<void>(
            room.emit(
                event));
      }
      else if constexpr (
          requires {
            room.broadcast(
                protocol::from_event(
                    event));
          })
      {
        static_cast<void>(
            room.broadcast(
                protocol::from_event(
                    event)));
      }
      else if constexpr (
          requires {
            room.broadcast_envelope(
                protocol::from_event(
                    event));
          })
      {
        static_cast<void>(
            room.broadcast_envelope(
                protocol::from_event(
                    event)));
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room broadcast API");
      }
    }

    struct SessionFixture
    {
      std::shared_ptr<Session> session{};
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
        std::string_view sessionId)
    {
      SessionFixture fixture;

      fixture.session =
          std::make_shared<Session>(
              SessionId{
                  sessionId});

      fixture.connection =
          std::make_shared<
              RecordingConnection>(
              std::string{
                  "connection-"} +
              std::string{
                  sessionId});

      fixture.session->attach(
          fixture.connection);

      return fixture;
    }

    [[nodiscard]] std::unique_ptr<Room>
    make_room()
    {
      auto room =
          std::make_unique<Room>(
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

    [[nodiscard]] RoomEvent make_event()
    {
      JsonObject payload;

      payload.set_i64(
          "value",
          42);

      RoomEvent event{
          make_room_id(),
          "counter.updated",
          std::move(payload),
          EventAudience::Room};

      event
          .set_event_id(
              EventId{
                  EventIdValue{7}})
          .set_room_version(
              RoomVersion{
                  VersionValue{7}})
          .set_request_id(
              RequestId{
                  "request-42"})
          .set_correlation_id(
              CorrelationId{
                  "correlation-84"});

      return event;
    }

    TEST(RoomBroadcastTest, RoomAudienceSendsToEveryConnectedMember)
    {
      auto room =
          make_room();

      SessionFixture first =
          make_session(
              "session-1");

      SessionFixture second =
          make_session(
              "session-2");

      SessionFixture third =
          make_session(
              "session-3");

      join_session(
          *room,
          first.session);

      join_session(
          *room,
          second.session);

      join_session(
          *room,
          third.session);

      RoomEvent event =
          make_event();

      event.set_audience(
          EventAudience::Room);

      broadcast_event(
          *room,
          event);

      EXPECT_EQ(
          first.connection
              ->send_count(),
          1U);

      EXPECT_EQ(
          second.connection
              ->send_count(),
          1U);

      EXPECT_EQ(
          third.connection
              ->send_count(),
          1U);
    }

    TEST(RoomBroadcastTest, SenderAudienceSendsOnlyToSourceSession)
    {
      auto room =
          make_room();

      SessionFixture sender =
          make_session(
              "session-1");

      SessionFixture other =
          make_session(
              "session-2");

      join_session(
          *room,
          sender.session);

      join_session(
          *room,
          other.session);

      RoomEvent event =
          make_event();

      event
          .set_source_session(
              sender.session->id())
          .set_audience(
              EventAudience::Sender);

      broadcast_event(
          *room,
          event);

      EXPECT_EQ(
          sender.connection
              ->send_count(),
          1U);

      EXPECT_EQ(
          other.connection
              ->send_count(),
          0U);
    }

    TEST(RoomBroadcastTest, OthersAudienceExcludesSourceSession)
    {
      auto room =
          make_room();

      SessionFixture sender =
          make_session(
              "session-1");

      SessionFixture second =
          make_session(
              "session-2");

      SessionFixture third =
          make_session(
              "session-3");

      join_session(
          *room,
          sender.session);

      join_session(
          *room,
          second.session);

      join_session(
          *room,
          third.session);

      RoomEvent event =
          make_event();

      event
          .set_source_session(
              sender.session->id())
          .set_audience(
              EventAudience::Others);

      broadcast_event(
          *room,
          event);

      EXPECT_EQ(
          sender.connection
              ->send_count(),
          0U);

      EXPECT_EQ(
          second.connection
              ->send_count(),
          1U);

      EXPECT_EQ(
          third.connection
              ->send_count(),
          1U);
    }

    TEST(RoomBroadcastTest, SessionAudienceSendsOnlyToTarget)
    {
      auto room =
          make_room();

      SessionFixture first =
          make_session(
              "session-1");

      SessionFixture target =
          make_session(
              "session-2");

      SessionFixture third =
          make_session(
              "session-3");

      join_session(
          *room,
          first.session);

      join_session(
          *room,
          target.session);

      join_session(
          *room,
          third.session);

      RoomEvent event =
          make_event();

      event
          .set_target_session(
              target.session->id())
          .set_audience(
              EventAudience::Session);

      broadcast_event(
          *room,
          event);

      EXPECT_EQ(
          first.connection
              ->send_count(),
          0U);

      EXPECT_EQ(
          target.connection
              ->send_count(),
          1U);

      EXPECT_EQ(
          third.connection
              ->send_count(),
          0U);
    }

    TEST(RoomBroadcastTest, InternalAudienceIsNotSentToSessions)
    {
      auto room =
          make_room();

      SessionFixture first =
          make_session(
              "session-1");

      SessionFixture second =
          make_session(
              "session-2");

      join_session(
          *room,
          first.session);

      join_session(
          *room,
          second.session);

      RoomEvent event =
          make_event();

      event.set_audience(
          EventAudience::Internal);

      broadcast_event(
          *room,
          event);

      EXPECT_EQ(
          first.connection
              ->send_count(),
          0U);

      EXPECT_EQ(
          second.connection
              ->send_count(),
          0U);
    }

    TEST(RoomBroadcastTest, DetachedSessionDoesNotReceiveEvent)
    {
      auto room =
          make_room();

      SessionFixture connected =
          make_session(
              "session-1");

      SessionFixture detached =
          make_session(
              "session-2");

      join_session(
          *room,
          connected.session);

      join_session(
          *room,
          detached.session);

      detached.session->detach();

      broadcast_event(
          *room,
          make_event());

      EXPECT_EQ(
          connected.connection
              ->send_count(),
          1U);

      EXPECT_EQ(
          detached.connection
              ->send_count(),
          0U);
    }

    TEST(RoomBroadcastTest, SessionThatLeftDoesNotReceiveEvent)
    {
      auto room =
          make_room();

      SessionFixture joined =
          make_session(
              "session-1");

      SessionFixture leaving =
          make_session(
              "session-2");

      join_session(
          *room,
          joined.session);

      join_session(
          *room,
          leaving.session);

      leave_session(
          *room,
          leaving.session);

      broadcast_event(
          *room,
          make_event());

      EXPECT_EQ(
          joined.connection
              ->send_count(),
          1U);

      EXPECT_EQ(
          leaving.connection
              ->send_count(),
          0U);
    }

    TEST(RoomBroadcastTest, BroadcastEnvelopePreservesEventData)
    {
      auto room =
          make_room();

      SessionFixture member =
          make_session(
              "session-1");

      join_session(
          *room,
          member.session);

      const RoomEvent event =
          make_event();

      broadcast_event(
          *room,
          event);

      ASSERT_EQ(
          member.connection
              ->send_count(),
          1U);

      const protocol::Envelope &envelope =
          member.connection
              ->last_envelope();

      EXPECT_EQ(
          envelope.kind(),
          protocol::MessageKind::Event);

      EXPECT_EQ(
          envelope.type(),
          "counter.updated");

      ASSERT_TRUE(
          envelope.room_id()
              .has_value());

      EXPECT_EQ(
          *envelope.room_id(),
          make_room_id());

      ASSERT_TRUE(
          envelope.room_version()
              .has_value());

      EXPECT_EQ(
          envelope.room_version()
              ->value(),
          VersionValue{7});

      ASSERT_TRUE(
          envelope.event_id()
              .has_value());

      EXPECT_EQ(
          envelope.event_id()
              ->value(),
          EventIdValue{7});

      EXPECT_EQ(
          envelope.request_id(),
          "request-42");

      EXPECT_EQ(
          envelope.correlation_id(),
          "correlation-84");

      const auto payload =
          vix::json::to_json(
              envelope.payload());

      EXPECT_EQ(
          payload.at("value")
              .get<std::int64_t>(),
          std::int64_t{42});
    }

  } // namespace

} // namespace vix::realtime
