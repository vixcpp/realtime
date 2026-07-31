/**
 *
 * @file session_resume_snapshot_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for snapshot fallback during Vix Realtime session resume.
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
      else
      {
        static_assert(
            dependentFalse<ConfigType>,
            "Unsupported replay limit configuration");
      }
    }

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
      snapshots() const
      {
        std::vector<protocol::Envelope>
            result;

        for (const auto &envelope :
             envelopes_)
        {
          if (envelope.kind() ==
              protocol::MessageKind::Snapshot)
          {
            result.push_back(
                envelope);
          }
        }

        return result;
      }

      [[nodiscard]] std::vector<
          protocol::Envelope>
      events() const
      {
        std::vector<protocol::Envelope>
            result;

        for (const auto &envelope :
             envelopes_)
        {
          if (envelope.kind() ==
              protocol::MessageKind::Event)
          {
            result.push_back(
                envelope);
          }
        }

        return result;
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
        JsonObject payload;

        payload.set_i64(
            "amount",
            1);

        RoomEvent event{
            command.room_id(),
            "counter.incremented",
            std::move(payload),
            EventAudience::Room};

        event
            .set_source_session(
                command.session_id())
            .set_request_id(
                command.request_id());

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
      static_cast<void>(
          manager.register_factory(
              "counter",
              factory));
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
      else
      {
        return *value;
      }
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
      else
      {
        static_cast<void>(
            room.join_session(
                session));
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
      else
      {
        session.update_room_cursor(
            roomId,
            eventId);
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
      else
      {
        return resume.resume(
            *session,
            connection,
            token,
            now);
      }
    }

    [[nodiscard]] RoomId make_room_id()
    {
      return RoomId{
          std::string_view{
              "counter/main"}};
    }

    [[nodiscard]] RoomCommand make_command(
        std::size_t index)
    {
      return RoomCommand{
          make_room_id(),
          SessionId{
              std::string_view{
                  "session-42"}},
          "counter.increment",
          JsonObject{},
          "request-" +
              std::to_string(
                  index)};
    }

    struct SnapshotFixture
    {
      Config config{};
      RoomManagerPtr manager{};
      std::unique_ptr<SessionResume>
          resume{};

      std::shared_ptr<Session>
          session{
              std::make_shared<Session>(
                  SessionId{
                      std::string_view{
                          "session-42"}},
                  Identity{
                      "user-42"})};

      SnapshotFixture()
      {
        config.snapshotEveryEvents = 3;
        config.snapshotOnRoomClose = false;
        config.snapshotsToKeep = 3;

        set_maximum_replay_events(
            config,
            2);

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

        if constexpr (
            std::constructible_from<
                SessionResume,
                RoomManagerPtr,
                std::chrono::milliseconds>)
        {
          resume =
              std::make_unique<
                  SessionResume>(
                  manager,
                  std::chrono::minutes{
                      5});
        }
        else
        {
          resume =
              std::make_unique<
                  SessionResume>(
                  manager);
        }

        session->attach(
            std::make_shared<
                RecordingConnection>(
                "connection-1"));
      }

      [[nodiscard]] std::shared_ptr<Room>
      open_populated_room()
      {
        auto room =
            room_handle(
                manager->open_room(
                    make_room_id(),
                    "counter"));

        join_session(
            *room,
            session);

        for (std::size_t index = 1;
             index <= 5;
             ++index)
        {
          EXPECT_TRUE(
              manager
                  ->execute(
                      make_command(
                          index))
                  .is_accepted());
        }

        return room;
      }

      [[nodiscard]] std::shared_ptr<
          RecordingConnection>
      resume_from(
          EventId eventId)
      {
        acknowledge_event(
            *session,
            make_room_id(),
            eventId);

        const ResumeToken token =
            issue_token(
                *resume,
                *session);

        const Timestamp now =
            SystemClock::now();

        detach_session(
            *session,
            now);

        auto connection =
            std::make_shared<
                RecordingConnection>(
                "connection-2");

        resume_session(
            *resume,
            session,
            connection,
            token,
            now +
                std::chrono::seconds{
                    1});

        return connection;
      }
    };

    TEST(SessionResumeSnapshotTest, CreatesExpectedStoredSnapshot)
    {
      SnapshotFixture fixture;

      fixture.open_populated_room();

      const auto snapshot =
          fixture.manager
              ->snapshot_store()
              ->load_latest(
                  make_room_id());

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot->room_version()
              .value(),
          VersionValue{3});

      EXPECT_EQ(
          snapshot->last_event_id()
              .value(),
          EventIdValue{3});
    }

    TEST(SessionResumeSnapshotTest, UsesSnapshotWhenReplayExceedsLimit)
    {
      SnapshotFixture fixture;

      fixture.open_populated_room();

      const auto connection =
          fixture.resume_from(
              EventId{});

      const auto snapshots =
          connection->snapshots();

      ASSERT_EQ(
          snapshots.size(),
          1U);

      ASSERT_TRUE(
          snapshots.front()
              .room_version()
              .has_value());

      EXPECT_EQ(
          snapshots.front()
              .room_version()
              ->value(),
          VersionValue{3});

      ASSERT_TRUE(
          snapshots.front()
              .event_id()
              .has_value());

      EXPECT_EQ(
          snapshots.front()
              .event_id()
              ->value(),
          EventIdValue{3});
    }

    TEST(SessionResumeSnapshotTest, SnapshotContainsSerializedState)
    {
      SnapshotFixture fixture;

      fixture.open_populated_room();

      const auto connection =
          fixture.resume_from(
              EventId{});

      const auto snapshots =
          connection->snapshots();

      ASSERT_EQ(
          snapshots.size(),
          1U);

      const auto payload =
          vix::json::to_json(
              snapshots.front()
                  .payload());

      EXPECT_EQ(
          payload.at("value")
              .get<std::int64_t>(),
          std::int64_t{3});
    }

    TEST(SessionResumeSnapshotTest, ReplaysEventsAfterSnapshot)
    {
      SnapshotFixture fixture;

      fixture.open_populated_room();

      const auto connection =
          fixture.resume_from(
              EventId{});

      const auto events =
          connection->events();

      ASSERT_EQ(
          events.size(),
          2U);

      ASSERT_TRUE(
          events[0].event_id().has_value());

      ASSERT_TRUE(
          events[1].event_id().has_value());

      EXPECT_EQ(
          events[0].event_id()->value(),
          EventIdValue{4});

      EXPECT_EQ(
          events[1].event_id()->value(),
          EventIdValue{5});
    }

    TEST(SessionResumeSnapshotTest, DoesNotUseSnapshotWhenReplayFitsLimit)
    {
      SnapshotFixture fixture;

      fixture.open_populated_room();

      const auto connection =
          fixture.resume_from(
              EventId{
                  EventIdValue{3}});

      EXPECT_TRUE(
          connection->snapshots()
              .empty());

      const auto events =
          connection->events();

      ASSERT_EQ(
          events.size(),
          2U);

      EXPECT_EQ(
          events[0].event_id()->value(),
          EventIdValue{4});

      EXPECT_EQ(
          events[1].event_id()->value(),
          EventIdValue{5});
    }

    TEST(SessionResumeSnapshotTest, SnapshotFallbackReachesLatestPosition)
    {
      SnapshotFixture fixture;

      fixture.open_populated_room();

      const auto connection =
          fixture.resume_from(
              EventId{});

      const auto snapshots =
          connection->snapshots();

      const auto events =
          connection->events();

      ASSERT_EQ(
          snapshots.size(),
          1U);

      ASSERT_EQ(
          events.size(),
          2U);

      EXPECT_EQ(
          snapshots.front()
              .event_id()
              ->value(),
          EventIdValue{3});

      EXPECT_EQ(
          events.back()
              .event_id()
              ->value(),
          EventIdValue{5});

      EXPECT_EQ(
          events.back()
              .room_version()
              ->value(),
          VersionValue{5});
    }

  } // namespace

} // namespace vix::realtime
