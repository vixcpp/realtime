/**
 *
 * @file realtime_session_flow_test.cpp
 * @author Gaspard Kirira
 * @brief Integration tests for the complete Vix Realtime session flow.
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
#include <optional>
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
          ErrorCode code,
          std::string_view reason) override
      {
        open_ = false;
        closeCode_ = code;
        closeReason_ = reason;
        ++closeCount_;
      }

      [[nodiscard]] JsonObject
      metadata() const override
      {
        return metadata_;
      }

      [[nodiscard]] std::size_t
      envelope_count() const noexcept
      {
        return envelopes_.size();
      }

      [[nodiscard]] const protocol::Envelope &
      envelope(
          std::size_t index) const
      {
        return envelopes_.at(
            index);
      }

      [[nodiscard]] std::vector<protocol::Envelope>
      event_envelopes() const
      {
        std::vector<protocol::Envelope>
            events;

        for (const protocol::Envelope &envelope :
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

      std::optional<ErrorCode>
          closeCode_{};

      std::string closeReason_{};
      std::size_t closeCount_{0};

      JsonObject metadata_{};

      std::vector<protocol::Envelope>
          envelopes_{};
    };

    class CounterState final : public RoomState
    {
    public:
      explicit CounterState(
          std::int64_t value = 0)
          : value_(
                value)
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
              "counter amount is required");
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

    class CounterFactory final : public RoomFactory
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

      [[nodiscard]] std::string_view
      room_type() const noexcept override
      {
        return "counter";
      }

      [[nodiscard]] RoomStatePtr create_state(
          const RoomId &) const override
      {
        return std::make_unique<CounterState>();
      }

      [[nodiscard]] RoomHandlerPtr create_handler(
          const RoomId &) const override
      {
        return std::make_unique<CounterHandler>();
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

    template <typename ConfigType>
    void set_maximum_replay_events(
        ConfigType &config,
        std::size_t value)
    {
      if constexpr (
          requires {
            config.maxReplayEvents =
                value;
          })
      {
        config.maxReplayEvents =
            value;
      }
      else if constexpr (
          requires {
            config.maximumReplayEvents =
                value;
          })
      {
        config.maximumReplayEvents =
            value;
      }
      else if constexpr (
          requires {
            config.maxEventsPerReplay =
                value;
          })
      {
        config.maxEventsPerReplay =
            value;
      }
    }

    template <typename ManagerType>
    void register_factory(
        ManagerType &manager,
        const CounterFactory &factory)
    {
      if constexpr (
          requires {
            manager.register_factory(
                std::make_shared<CounterFactory>(
                    factory));
          })
      {
        static_cast<void>(
            manager.register_factory(
                std::make_shared<CounterFactory>(
                    factory)));
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
      else if constexpr (
          requires {
            manager.register_factory(
                std::string{
                    "counter"},
                factory);
          })
      {
        static_cast<void>(
            manager.register_factory(
                std::string{
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
          std::remove_cvref_t<
              ValueType>;

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
      if constexpr (
          requires {
            manager.open_room(
                roomId,
                std::string_view{
                    "counter"});
          })
      {
        return room_handle(
            manager.open_room(
                roomId,
                std::string_view{
                    "counter"}));
      }
      else if constexpr (
          requires {
            manager.open_room(
                roomId,
                std::string{
                    "counter"});
          })
      {
        return room_handle(
            manager.open_room(
                roomId,
                std::string{
                    "counter"}));
      }
      else
      {
        static_assert(
            dependentFalse<ManagerType>,
            "Unsupported RoomManager open API");
      }
    }

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
              room.join(session->id())
            } -> std::convertible_to<bool>;
          })
      {
        return room.join(
            session->id());
      }
      else if constexpr (
          requires {
            room.join(session->id());
          })
      {
        room.join(
            session->id());

        return true;
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
            room.leave_session(
                sessionId);
          })
      {
        room.leave_session(
            sessionId);

        return true;
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
        const std::shared_ptr<RoomManager>
            &manager)
    {
      if constexpr (
          std::constructible_from<
              ResumeType,
              std::shared_ptr<RoomManager>,
              std::chrono::milliseconds>)
      {
        return std::make_unique<ResumeType>(
            manager,
            std::chrono::minutes{
                5});
      }
      else if constexpr (
          std::constructible_from<
              ResumeType,
              std::shared_ptr<RoomManager>,
              std::chrono::seconds>)
      {
        return std::make_unique<ResumeType>(
            manager,
            std::chrono::minutes{
                5});
      }
      else if constexpr (
          std::constructible_from<
              ResumeType,
              std::shared_ptr<RoomManager>>)
      {
        return std::make_unique<ResumeType>(
            manager);
      }
      else
      {
        static_assert(
            dependentFalse<ResumeType>,
            "Unsupported SessionResume constructor API");
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
      else if constexpr (
          requires {
            resume.issue(session);
            session.resume_token();
          })
      {
        resume.issue(
            session);

        return session.resume_token();
      }
      else
      {
        static_assert(
            dependentFalse<ResumeType>,
            "Unsupported SessionResume token API");
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
      else if constexpr (
          requires {
            session.detach();
          })
      {
        session.detach();
      }
      else
      {
        static_assert(
            dependentFalse<SessionType>,
            "Unsupported Session detach API");
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
            {
              resume.resume(
                  session,
                  connection,
                  token,
                  now)
            } -> std::same_as<
                SessionResumeResult>;
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
            {
              resume.resume(
                  *session,
                  connection,
                  token,
                  now)
            } -> std::same_as<
                SessionResumeResult>;
          })
      {
        return resume.resume(
            *session,
            connection,
            token,
            now);
      }
      else if constexpr (
          requires {
            {
              resume.resume(
                  session,
                  connection,
                  token)
            } -> std::same_as<
                SessionResumeResult>;
          })
      {
        return resume.resume(
            session,
            connection,
            token);
      }
      else
      {
        static_assert(
            dependentFalse<ResumeType>,
            "Unsupported SessionResume resume API");
      }
    }

    template <typename ManagerType>
    void shutdown_manager(
        ManagerType &manager)
    {
      if constexpr (
          requires {
            manager.shutdown();
          })
      {
        static_cast<void>(
            manager.shutdown());
      }
      else if constexpr (
          requires {
            manager.close_all();
          })
      {
        static_cast<void>(
            manager.close_all());
      }
      else if constexpr (
          requires {
            manager.stop();
          })
      {
        static_cast<void>(
            manager.stop());
      }
      else
      {
        static_assert(
            dependentFalse<ManagerType>,
            "Unsupported RoomManager shutdown API");
      }
    }

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
        std::int64_t amount,
        std::string requestId)
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
          std::move(requestId)};

      command.set_correlation_id(
          CorrelationId{
              "correlation-42"});

      return command;
    }

    void deliver_result(
        const CommandResult &result,
        const ConnectionPtr &connection)
    {
      if (!result.is_accepted())
      {
        return;
      }

      for (const RoomEvent &event :
           result.events())
      {
        connection->send(
            protocol::from_event(
                event));
      }
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

    struct RealtimeFixture
    {
      Config config{};

      std::shared_ptr<RoomManager>
          manager{};

      std::unique_ptr<SessionResume>
          resume{};

      std::shared_ptr<Session>
          session{};

      std::shared_ptr<RecordingConnection>
          firstConnection{};

      RealtimeFixture()
      {
        config.maxActiveRooms = 16;
        config.maxPendingCommandsPerRoom = 16;
        config.snapshotEveryEvents = 0;
        config.snapshotOnRoomClose = false;
        config.restoreRoomsOnOpen = true;

        set_maximum_replay_events(
            config,
            100);

        manager =
            std::make_shared<RoomManager>(
                NodeId{
                    std::string_view{
                        "node-1"}},
                config);

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

        session =
            std::make_shared<Session>(
                make_session_id(),
                Identity{
                    "user-42"});

        firstConnection =
            std::make_shared<
                RecordingConnection>(
                "connection-1");

        session->attach(
            firstConnection);
      }
    };

    TEST(
        RealtimeSessionFlowTest,
        OpensRoomAndJoinsConnectedSession)
    {
      RealtimeFixture fixture;

      const auto room =
          open_room(
              *fixture.manager,
              make_room_id());

      ASSERT_NE(
          room,
          nullptr);

      ASSERT_TRUE(
          join_session(
              *room,
              fixture.session));

      EXPECT_TRUE(
          room->is_open());

      EXPECT_EQ(
          room->member_count(),
          1U);

      EXPECT_TRUE(
          has_session(
              *room,
              fixture.session->id()));

      EXPECT_TRUE(
          fixture.session->has_room(
              room->id()));

      EXPECT_EQ(
          fixture.session->room_count(),
          1U);
    }

    TEST(
        RealtimeSessionFlowTest,
        CommandUpdatesRoomPersistsAndDeliversEvent)
    {
      RealtimeFixture fixture;

      const auto room =
          open_room(
              *fixture.manager,
              make_room_id());

      ASSERT_NE(
          room,
          nullptr);

      ASSERT_TRUE(
          join_session(
              *room,
              fixture.session));

      const CommandResult result =
          fixture.manager->execute(
              make_command(
                  "counter.increment",
                  5,
                  "request-1"));

      ASSERT_TRUE(
          result.is_accepted());

      deliver_result(
          result,
          fixture.firstConnection);

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{5});

      EXPECT_EQ(
          room->version()
              .value(),
          VersionValue{1});

      EXPECT_EQ(
          room->last_event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          fixture.manager
              ->event_store()
              ->count(
                  make_room_id()),
          1U);

      const auto envelopes =
          fixture.firstConnection
              ->event_envelopes();

      ASSERT_EQ(
          envelopes.size(),
          1U);

      EXPECT_EQ(
          envelopes.front()
              .type(),
          "counter.incremented");

      EXPECT_EQ(
          envelopes.front()
              .request_id(),
          "request-1");

      EXPECT_EQ(
          envelopes.front()
              .correlation_id(),
          "correlation-42");

      ASSERT_TRUE(
          envelopes.front()
              .event_id()
              .has_value());

      EXPECT_EQ(
          envelopes.front()
              .event_id()
              ->value(),
          EventIdValue{1});
    }

    TEST(
        RealtimeSessionFlowTest,
        DetachedSessionResumesAndReceivesMissingEvents)
    {
      RealtimeFixture fixture;

      const auto room =
          open_room(
              *fixture.manager,
              make_room_id());

      ASSERT_NE(
          room,
          nullptr);

      ASSERT_TRUE(
          join_session(
              *room,
              fixture.session));

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      "counter.increment",
                      1,
                      "request-1"))
              .is_accepted());

      acknowledge_event(
          *fixture.session,
          make_room_id(),
          EventId{
              EventIdValue{1}});

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      ASSERT_FALSE(
          token.empty());

      const Timestamp detachedAt =
          SystemClock::now();

      detach_session(
          *fixture.session,
          detachedAt);

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      "counter.increment",
                      2,
                      "request-2"))
              .is_accepted());

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      "counter.increment",
                      3,
                      "request-3"))
              .is_accepted());

      const auto secondConnection =
          std::make_shared<
              RecordingConnection>(
              "connection-2");

      static_cast<void>(
          resume_session(
              *fixture.resume,
              fixture.session,
              secondConnection,
              token,
              detachedAt +
                  std::chrono::seconds{
                      1}));

      const auto replayed =
          secondConnection
              ->event_envelopes();

      ASSERT_EQ(
          replayed.size(),
          2U);

      ASSERT_TRUE(
          replayed[0].event_id().has_value());

      ASSERT_TRUE(
          replayed[1].event_id().has_value());

      EXPECT_EQ(
          replayed[0].event_id()->value(),
          EventIdValue{2});

      EXPECT_EQ(
          replayed[1].event_id()->value(),
          EventIdValue{3});

      EXPECT_EQ(
          replayed[0].type(),
          "counter.incremented");

      EXPECT_EQ(
          replayed[1].type(),
          "counter.incremented");

      EXPECT_TRUE(
          secondConnection->is_open());

      EXPECT_TRUE(
          has_session(
              *room,
              fixture.session->id()));

      EXPECT_TRUE(
          fixture.session->has_room(
              room->id()));

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{6});
    }

    TEST(
        RealtimeSessionFlowTest,
        SessionCanLeaveRoomAfterResume)
    {
      RealtimeFixture fixture;

      const auto room =
          open_room(
              *fixture.manager,
              make_room_id());

      ASSERT_NE(
          room,
          nullptr);

      ASSERT_TRUE(
          join_session(
              *room,
              fixture.session));

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      const Timestamp detachedAt =
          SystemClock::now();

      detach_session(
          *fixture.session,
          detachedAt);

      const auto secondConnection =
          std::make_shared<
              RecordingConnection>(
              "connection-2");

      static_cast<void>(
          resume_session(
              *fixture.resume,
              fixture.session,
              secondConnection,
              token,
              detachedAt +
                  std::chrono::seconds{
                      1}));

      ASSERT_TRUE(
          has_session(
              *room,
              fixture.session->id()));

      EXPECT_TRUE(
          leave_session(
              *room,
              fixture.session->id()));

      EXPECT_EQ(
          room->member_count(),
          0U);

      EXPECT_TRUE(
          room->empty());

      EXPECT_FALSE(
          has_session(
              *room,
              fixture.session->id()));

      EXPECT_FALSE(
          fixture.session->has_room(
              room->id()));

      EXPECT_EQ(
          fixture.session->room_count(),
          0U);
    }

    TEST(
        RealtimeSessionFlowTest,
        ShutdownClosesRoomAndPreservesEvents)
    {
      RealtimeFixture fixture;

      const auto room =
          open_room(
              *fixture.manager,
              make_room_id());

      ASSERT_NE(
          room,
          nullptr);

      ASSERT_TRUE(
          join_session(
              *room,
              fixture.session));

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      "counter.increment",
                      5,
                      "request-1"))
              .is_accepted());

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      "counter.decrement",
                      2,
                      "request-2"))
              .is_accepted());

      ASSERT_EQ(
          fixture.manager
              ->event_store()
              ->count(
                  make_room_id()),
          2U);

      shutdown_manager(
          *fixture.manager);

      EXPECT_TRUE(
          room->is_closed());

      EXPECT_FALSE(
          room->is_open());

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);

      EXPECT_EQ(
          fixture.manager
              ->event_store()
              ->count(
                  make_room_id()),
          2U);

      EXPECT_EQ(
          fixture.manager
              ->event_store()
              ->latest_event_id(
                  make_room_id())
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{3});
    }

  } // namespace

} // namespace vix::realtime
