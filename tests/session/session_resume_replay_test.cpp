/**
 *
 * @file session_resume_replay_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for event replay during Vix Realtime session resume.
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
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <vix/json/json.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_store.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_manager.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/session.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/session_resume.hpp>
#include <vix/realtime/snapshot_store.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    template <typename>
    inline constexpr bool dependentFalse = false;

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
          ErrorCode,
          std::string_view) override
      {
        open_ = false;
      }

      [[nodiscard]] JsonObject
      metadata() const noexcept override
      {
        return metadata_;
      }

      [[nodiscard]] std::vector<
          protocol::Envelope>
      event_envelopes() const
      {
        std::vector<protocol::Envelope>
            events;

        for (const auto &envelope :
             envelopes_)
        {
          if (envelope.kind() ==
              protocol::MessageKind::Event)
          {
            events.push_back(
                envelope);
          }
        }

        return events;
      }

    private:
      ConnectionId identifier_{};
      bool open_{true};
      JsonObject metadata_{};

      std::vector<protocol::Envelope>
          envelopes_{};
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

        value_ +=
            payload.at("amount")
                .get<std::int64_t>();
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
        const auto payload =
            vix::json::to_json(
                command.payload());

        JsonObject eventPayload;

        eventPayload.set_i64(
            "amount",
            payload.at("amount")
                .get<std::int64_t>());

        RoomEvent event{
            command.room_id(),
            "counter.incremented",
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

    class CounterFactory
    {
    public:
      CounterFactory(
          EventStorePtr eventStore,
          SnapshotStorePtr snapshotStore,
          Config config)
          : eventStore_(
                std::move(eventStore)),
            snapshotStore_(
                std::move(snapshotStore)),
            config_(
                std::move(config))
      {
      }

      [[nodiscard]] std::shared_ptr<Room>
      operator()(
          const RoomId &roomId) const
      {
        return create(
            roomId,
            config_);
      }

      [[nodiscard]] std::shared_ptr<Room>
      operator()(
          const RoomId &roomId,
          const Config &config) const
      {
        return create(
            roomId,
            config);
      }

    private:
      [[nodiscard]] std::shared_ptr<Room>
      create(
          const RoomId &roomId,
          const Config &config) const
      {
        return std::make_shared<Room>(
            roomId,
            std::make_unique<CounterState>(),
            std::make_unique<CounterHandler>(),
            eventStore_,
            snapshotStore_,
            config);
      }

      EventStorePtr eventStore_{};
      SnapshotStorePtr snapshotStore_{};
      Config config_{};
    };

    template <typename ManagerType>
    void register_factory(
        ManagerType &manager,
        const CounterFactory &factory)
    {
      if constexpr (
          requires {
            manager.register_factory(
                "counter",
                factory);
          })
      {
        static_cast<void>(
            manager.register_factory(
                "counter",
                factory));
      }
      else if constexpr (
          requires {
            manager.register_factory(
                std::string_view{
                    "counter"},
                factory);
          })
      {
        static_cast<void>(
            manager.register_factory(
                std::string_view{
                    "counter"},
                factory));
      }
      else
      {
        static_assert(
            dependentFalse<ManagerType>,
            "Unsupported RoomManager factory API");
      }
    }

    template <typename ValueType>
    [[nodiscard]] std::shared_ptr<Room>
    room_handle(
        ValueType &&value)
    {
      using Value =
          std::remove_cvref_t<ValueType>;

      if constexpr (
          std::same_as<
              Value,
              std::shared_ptr<Room>>)
      {
        return std::forward<ValueType>(
            value);
      }
      else if constexpr (
          std::same_as<
              Value,
              std::weak_ptr<Room>>)
      {
        return value.lock();
      }
      else if constexpr (
          requires {
            value.has_value();
            *value;
          })
      {
        if (!value.has_value())
        {
          return nullptr;
        }

        return room_handle(
            *value);
      }
      else
      {
        static_assert(
            dependentFalse<Value>,
            "Unsupported RoomManager room handle");
      }
    }

    template <typename ManagerType>
    [[nodiscard]] std::shared_ptr<Room>
    open_room(
        ManagerType &manager,
        const RoomId &roomId)
    {
      return room_handle(
          manager.open_room(
              roomId,
              "counter"));
    }

    template <typename RoomType>
    void join_session(
        RoomType &room,
        const std::shared_ptr<Session> &session)
    {
      if constexpr (
          requires {
            room.join(
                session);
          })
      {
        static_cast<void>(
            room.join(
                session));
      }
      else if constexpr (
          requires {
            room.join_session(
                session);
          })
      {
        static_cast<void>(
            room.join_session(
                session));
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room join API");
      }
    }

    template <typename SessionType>
    void acknowledge_event(
        SessionType &session,
        const RoomId &roomId,
        EventId eventId)
    {
      if constexpr (
          requires {
            session.acknowledge(
                roomId,
                eventId);
          })
      {
        session.acknowledge(
            roomId,
            eventId);
      }
      else if constexpr (
          requires {
            session.ack(
                roomId,
                eventId);
          })
      {
        session.ack(
            roomId,
            eventId);
      }
      else if constexpr (
          requires {
            session.set_room_cursor(
                roomId,
                eventId);
          })
      {
        session.set_room_cursor(
            roomId,
            eventId);
      }
      else if constexpr (
          requires {
            session.update_room_cursor(
                roomId,
                eventId);
          })
      {
        session.update_room_cursor(
            roomId,
            eventId);
      }
      else if constexpr (
          requires {
            session.set_last_event_id(
                roomId,
                eventId);
          })
      {
        session.set_last_event_id(
            roomId,
            eventId);
      }
      else
      {
        static_assert(
            dependentFalse<SessionType>,
            "Unsupported Session acknowledgement API");
      }
    }

    template <typename ResumeType>
    [[nodiscard]] std::unique_ptr<ResumeType>
    make_session_resume(
        const RoomManagerPtr &manager)
    {
      if constexpr (
          std::constructible_from<
              ResumeType,
              RoomManagerPtr,
              std::chrono::milliseconds>)
      {
        return std::make_unique<ResumeType>(
            manager,
            std::chrono::minutes{
                5});
      }
      else
      {
        return std::make_unique<ResumeType>(
            manager);
      }
    }

    template <typename ResumeType>
    [[nodiscard]] ResumeToken issue_token(
        ResumeType &resume,
        Session &session)
    {
      if constexpr (
          requires {
            {
              resume.issue(session)
            } -> std::convertible_to<
                ResumeToken>;
          })
      {
        return resume.issue(
            session);
      }
      else
      {
        resume.issue(
            session);

        return session.resume_token();
      }
    }

    template <typename SessionType>
    void detach_session(
        SessionType &session,
        Timestamp now)
    {
      if constexpr (
          requires {
            session.detach(
                now);
          })
      {
        session.detach(
            now);
      }
      else
      {
        session.detach();
      }
    }

    template <typename ResumeType>
    [[nodiscard]] SessionResumeResult
    resume_session(
        ResumeType &resume,
        const std::shared_ptr<Session> &session,
        const ConnectionPtr &connection,
        const ResumeToken &token,
        Timestamp now)
    {
      if constexpr (
          requires {
            resume.resume(
                session,
                connection,
                token,
                now);
          })
      {
        return resume.resume(
            session,
            connection,
            token,
            now);
      }
      else if constexpr (
          requires {
            resume.resume(
                *session,
                connection,
                token,
                now);
          })
      {
        return resume.resume(
            *session,
            connection,
            token,
            now);
      }
      else
      {
        return resume.resume(
            session,
            connection,
            token);
      }
    }

    [[nodiscard]] RoomId make_room_id(
        std::string_view value =
            "counter/main")
    {
      return RoomId{
          value};
    }

    [[nodiscard]] RoomCommand make_command(
        const RoomId &roomId,
        std::int64_t amount,
        std::string requestId)
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      return RoomCommand{
          roomId,
          SessionId{
              std::string_view{
                  "session-42"}},
          "counter.increment",
          std::move(payload),
          std::move(requestId)};
    }

    struct ReplayFixture
    {
      Config config{};

      RoomManagerPtr manager{
          std::make_shared<RoomManager>(
              NodeId{
                  std::string_view{
                      "node-1"}},
              config)};

      std::unique_ptr<SessionResume>
          resume{};

      std::shared_ptr<Session>
          session{};

      std::shared_ptr<RecordingConnection>
          previousConnection{
              std::make_shared<
                  RecordingConnection>(
                  "connection-1")};

      explicit ReplayFixture(
          std::size_t maxReplayEvents = 1000,
          std::size_t maxReplayBytes = 4 * 1024 * 1024)
      {
        config.maxReplayEvents = maxReplayEvents;
        config.maxReplayBytes = maxReplayBytes;
        config.snapshotEveryEvents = 0;
        config.snapshotOnRoomClose = false;

        manager =
            std::make_shared<RoomManager>(
                NodeId{
                    std::string_view{
                        "node-1"}},
                config);

        session = manager->create_session(
            SessionId{
                std::string_view{
                    "session-42"}},
            Identity{
                "user-42"});

        register_factory(
            *manager,
            CounterFactory{
                manager->event_store(),
                manager->snapshot_store(),
                config});

        resume =
            make_session_resume<
                SessionResume>(
                manager);

        session->attach(
            previousConnection);
      }
    };

    TEST(SessionResumeReplayTest, ReplaysOnlyMissingEvents)
    {
      ReplayFixture fixture;

      const RoomId roomId =
          make_room_id();

      const auto room =
          open_room(
              *fixture.manager,
              roomId);

      ASSERT_NE(
          room,
          nullptr);

      join_session(
          *room,
          fixture.session);

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      roomId,
                      1,
                      "request-1"))
              .is_accepted());

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      roomId,
                      1,
                      "request-2"))
              .is_accepted());

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      roomId,
                      1,
                      "request-3"))
              .is_accepted());

      acknowledge_event(
          *fixture.session,
          roomId,
          EventId{
              EventIdValue{1}});

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      const Timestamp now =
          SystemClock::now();

      detach_session(
          *fixture.session,
          now);

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-2");

      resume_session(
          *fixture.resume,
          fixture.session,
          connection,
          token,
          now +
              std::chrono::seconds{
                  1});

      const auto events =
          connection->event_envelopes();

      ASSERT_EQ(
          events.size(),
          2U);

      ASSERT_TRUE(
          events[0].event_id().has_value());

      ASSERT_TRUE(
          events[1].event_id().has_value());

      EXPECT_EQ(
          events[0].event_id()->value(),
          EventIdValue{2});

      EXPECT_EQ(
          events[1].event_id()->value(),
          EventIdValue{3});
    }

    TEST(SessionResumeReplayTest, PreservesReplayOrder)
    {
      ReplayFixture fixture;

      const RoomId roomId =
          make_room_id();

      const auto room =
          open_room(
              *fixture.manager,
              roomId);

      join_session(
          *room,
          fixture.session);

      for (std::int64_t amount :
           {1, 2, 3})
      {
        ASSERT_TRUE(
            fixture.manager
                ->execute(
                    make_command(
                        roomId,
                        amount,
                        "request-" +
                            std::to_string(
                                amount)))
                .is_accepted());
      }

      acknowledge_event(
          *fixture.session,
          roomId,
          EventId{});

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      const Timestamp now =
          SystemClock::now();

      detach_session(
          *fixture.session,
          now);

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-2");

      resume_session(
          *fixture.resume,
          fixture.session,
          connection,
          token,
          now);

      const auto events =
          connection->event_envelopes();

      ASSERT_EQ(
          events.size(),
          3U);

      EXPECT_EQ(
          events[0].event_id()->value(),
          EventIdValue{1});

      EXPECT_EQ(
          events[1].event_id()->value(),
          EventIdValue{2});

      EXPECT_EQ(
          events[2].event_id()->value(),
          EventIdValue{3});
    }

    TEST(SessionResumeReplayTest, SendsNoEventsWhenCursorIsCurrent)
    {
      ReplayFixture fixture;

      const RoomId roomId =
          make_room_id();

      const auto room =
          open_room(
              *fixture.manager,
              roomId);

      join_session(
          *room,
          fixture.session);

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      roomId,
                      1,
                      "request-1"))
              .is_accepted());

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      roomId,
                      1,
                      "request-2"))
              .is_accepted());

      acknowledge_event(
          *fixture.session,
          roomId,
          EventId{
              EventIdValue{2}});

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      const Timestamp now =
          SystemClock::now();

      detach_session(
          *fixture.session,
          now);

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-2");

      resume_session(
          *fixture.resume,
          fixture.session,
          connection,
          token,
          now);

      EXPECT_TRUE(
          connection
              ->event_envelopes()
              .empty());
    }

    TEST(SessionResumeReplayTest, ReplaysJoinedRoomsIndependently)
    {
      ReplayFixture fixture;

      const RoomId firstRoomId =
          make_room_id(
              "counter/first");

      const RoomId secondRoomId =
          make_room_id(
              "counter/second");

      const auto firstRoom =
          open_room(
              *fixture.manager,
              firstRoomId);

      const auto secondRoom =
          open_room(
              *fixture.manager,
              secondRoomId);

      join_session(
          *firstRoom,
          fixture.session);

      join_session(
          *secondRoom,
          fixture.session);

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      firstRoomId,
                      1,
                      "request-1"))
              .is_accepted());

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      firstRoomId,
                      1,
                      "request-2"))
              .is_accepted());

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      secondRoomId,
                      1,
                      "request-3"))
              .is_accepted());

      acknowledge_event(
          *fixture.session,
          firstRoomId,
          EventId{
              EventIdValue{1}});

      acknowledge_event(
          *fixture.session,
          secondRoomId,
          EventId{});

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      const Timestamp now =
          SystemClock::now();

      detach_session(
          *fixture.session,
          now);

      const auto connection =
          std::make_shared<
              RecordingConnection>(
              "connection-2");

      resume_session(
          *fixture.resume,
          fixture.session,
          connection,
          token,
          now);

      const auto events =
          connection->event_envelopes();

      ASSERT_EQ(
          events.size(),
          2U);

      EXPECT_EQ(
          events[0].room_id(),
          std::optional<RoomId>{
              firstRoomId});

      EXPECT_EQ(
          events[1].room_id(),
          std::optional<RoomId>{
              secondRoomId});
    }

    TEST(SessionResumeReplayTest, RejectsIncompleteReplayWithoutSnapshot)
    {
      ReplayFixture fixture{1};
      const RoomId roomId = make_room_id();
      const auto room = open_room(*fixture.manager, roomId);
      join_session(*room, fixture.session);

      ASSERT_TRUE(fixture.manager->execute(make_command(roomId, 1, "request-1")).is_accepted());
      ASSERT_TRUE(fixture.manager->execute(make_command(roomId, 1, "request-2")).is_accepted());

      const ResumeToken token = issue_token(*fixture.resume, *fixture.session);
      const Timestamp now = SystemClock::now();
      detach_session(*fixture.session, now);
      const auto connection = std::make_shared<RecordingConnection>("connection-2");

      EXPECT_THROW(
          static_cast<void>(resume_session(*fixture.resume, fixture.session, connection, token, now)),
          Error);
      EXPECT_FALSE(fixture.session->connected());
      EXPECT_TRUE(fixture.session->last_event_id(roomId).empty());
    }

    TEST(SessionResumeReplayTest, RejectsReplayThatExceedsByteLimit)
    {
      ReplayFixture fixture{1000, 1};
      const RoomId roomId = make_room_id();
      const auto room = open_room(*fixture.manager, roomId);
      join_session(*room, fixture.session);
      ASSERT_TRUE(fixture.manager->execute(make_command(roomId, 1, "request-1")).is_accepted());

      const ResumeToken token = issue_token(*fixture.resume, *fixture.session);
      const Timestamp now = SystemClock::now();
      detach_session(*fixture.session, now);
      const auto connection = std::make_shared<RecordingConnection>("connection-2");

      EXPECT_THROW(
          static_cast<void>(resume_session(*fixture.resume, fixture.session, connection, token, now)),
          Error);
      EXPECT_FALSE(fixture.session->connected());
      EXPECT_TRUE(fixture.session->last_event_id(roomId).empty());
    }

    TEST(SessionResumeReplayTest, LaterRoomFailureRollsBackEveryEarlierReplayChange)
    {
      ReplayFixture fixture{1};
      const RoomId firstRoomId = make_room_id("counter/first");
      const RoomId secondRoomId = make_room_id("counter/second");
      const auto firstRoom = open_room(*fixture.manager, firstRoomId);
      const auto secondRoom = open_room(*fixture.manager, secondRoomId);

      join_session(*firstRoom, fixture.session);
      join_session(*secondRoom, fixture.session);

      ASSERT_TRUE(
          fixture.manager->execute(
              make_command(firstRoomId, 1, "first-request"))
              .is_accepted());
      ASSERT_TRUE(
          fixture.manager->execute(
              make_command(secondRoomId, 1, "second-request-1"))
              .is_accepted());
      ASSERT_TRUE(
          fixture.manager->execute(
              make_command(secondRoomId, 1, "second-request-2"))
              .is_accepted());

      const ResumeToken token = issue_token(*fixture.resume, *fixture.session);
      const Timestamp now = SystemClock::now();
      detach_session(*fixture.session, now);
      const auto connection = std::make_shared<RecordingConnection>("connection-2");

      EXPECT_THROW(
          static_cast<void>(
              resume_session(
                  *fixture.resume,
                  fixture.session,
                  connection,
                  token,
                  now + std::chrono::seconds{1})),
          Error);

      EXPECT_FALSE(fixture.session->connected());
      EXPECT_EQ(fixture.session->resume_token(), token);
      EXPECT_TRUE(fixture.session->last_event_id(firstRoomId).empty());
      EXPECT_TRUE(fixture.session->last_event_id(secondRoomId).empty());
      EXPECT_TRUE(connection->is_open());

      const auto presenceStore = fixture.manager->presence_store();
      ASSERT_NE(presenceStore, nullptr);
      EXPECT_FALSE(
          presenceStore->find(firstRoomId, fixture.session->id()).has_value());
      EXPECT_FALSE(
          presenceStore->find(secondRoomId, fixture.session->id()).has_value());
    }

  } // namespace

} // namespace vix::realtime
