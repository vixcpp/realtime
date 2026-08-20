/**
 *
 * @file room_manager_routing_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for command routing through the Vix Realtime room manager.
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
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_store.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/presence.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_manager.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/server.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/snapshot_store.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    template <typename>
    inline constexpr bool dependentFalse = false;

    class CounterState final : public RoomState
    {
    public:
      explicit CounterState(
          std::int64_t value = 0)
          : value_(value)
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

    struct FactoryProbe
    {
      std::size_t callCount{0};
      std::vector<RoomId> createdRooms{};
    };

    class SharedCounterFactory
    {
    public:
      SharedCounterFactory(
          EventStorePtr eventStore,
          SnapshotStorePtr snapshotStore,
          Config config,
          std::shared_ptr<FactoryProbe> probe)
          : eventStore_(
                std::move(eventStore)),
            snapshotStore_(
                std::move(snapshotStore)),
            config_(
                std::move(config)),
            probe_(
                std::move(probe))
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
        ++probe_->callCount;

        probe_->createdRooms.push_back(
            roomId);

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
      std::shared_ptr<FactoryProbe> probe_{};
    };

    class UniqueCounterFactory
    {
    public:
      UniqueCounterFactory(
          EventStorePtr eventStore,
          SnapshotStorePtr snapshotStore,
          Config config,
          std::shared_ptr<FactoryProbe> probe)
          : eventStore_(
                std::move(eventStore)),
            snapshotStore_(
                std::move(snapshotStore)),
            config_(
                std::move(config)),
            probe_(
                std::move(probe))
      {
      }

      [[nodiscard]] std::unique_ptr<Room>
      operator()(
          const RoomId &roomId) const
      {
        return create(
            roomId,
            config_);
      }

      [[nodiscard]] std::unique_ptr<Room>
      operator()(
          const RoomId &roomId,
          const Config &config) const
      {
        return create(
            roomId,
            config);
      }

    private:
      [[nodiscard]] std::unique_ptr<Room>
      create(
          const RoomId &roomId,
          const Config &config) const
      {
        ++probe_->callCount;

        probe_->createdRooms.push_back(
            roomId);

        return std::make_unique<Room>(
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
      std::shared_ptr<FactoryProbe> probe_{};
    };

    template <
        typename ManagerType,
        typename SharedFactory,
        typename UniqueFactory>
    void register_counter_factory(
        ManagerType &manager,
        std::string_view factoryType,
        const SharedFactory &sharedFactory,
        const UniqueFactory &uniqueFactory)
    {
      if constexpr (
          requires {
            manager.register_factory(
                factoryType,
                sharedFactory);
          })
      {
        static_cast<void>(
            manager.register_factory(
                factoryType,
                sharedFactory));
      }
      else if constexpr (
          requires {
            manager.register_factory(
                std::string{
                    factoryType},
                sharedFactory);
          })
      {
        static_cast<void>(
            manager.register_factory(
                std::string{
                    factoryType},
                sharedFactory));
      }
      else if constexpr (
          requires {
            manager.register_factory(
                factoryType,
                uniqueFactory);
          })
      {
        static_cast<void>(
            manager.register_factory(
                factoryType,
                uniqueFactory));
      }
      else if constexpr (
          requires {
            manager.register_factory(
                std::string{
                    factoryType},
                uniqueFactory);
          })
      {
        static_cast<void>(
            manager.register_factory(
                std::string{
                    factoryType},
                uniqueFactory));
      }
      else
      {
        static_assert(
            dependentFalse<ManagerType>,
            "Unsupported RoomManager factory API");
      }
    }

    template <typename ValueType>
    [[nodiscard]] Room *room_pointer(
        ValueType &&value)
    {
      using Value =
          std::remove_cvref_t<
              ValueType>;

      if constexpr (
          std::is_pointer_v<Value>)
      {
        return value;
      }
      else if constexpr (
          std::same_as<
              Value,
              Room>)
      {
        return std::addressof(
            value);
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

        return room_pointer(
            *value);
      }
      else if constexpr (
          requires {
            value.get();
          })
      {
        return value.get();
      }
      else if constexpr (
          requires {
            value.lock();
          })
      {
        return value.lock()
            .get();
      }
      else
      {
        static_assert(
            dependentFalse<Value>,
            "Unsupported managed room handle");
      }
    }

    template <typename ManagerType>
    [[nodiscard]] Room *open_room(
        ManagerType &manager,
        const RoomId &roomId,
        std::string_view factoryType)
    {
      Room *room = nullptr;

      if constexpr (
          requires {
            manager.open_room(
                roomId,
                factoryType);
          })
      {
        decltype(auto) result =
            manager.open_room(
                roomId,
                factoryType);

        room = room_pointer(result);
      }
      else if constexpr (
          requires {
            manager.open_room(
                roomId,
                std::string{
                    factoryType});
          })
      {
        decltype(auto) result =
            manager.open_room(
                roomId,
                std::string{
                    factoryType});

        room = room_pointer(result);
      }
      else if constexpr (
          requires {
            manager.open_room(
                factoryType,
                roomId);
          })
      {
        decltype(auto) result =
            manager.open_room(
                factoryType,
                roomId);

        room = room_pointer(result);
      }
      else
      {
        static_assert(
            dependentFalse<ManagerType>,
            "Unsupported RoomManager open_room API");
      }

      if (room)
      {
        static_cast<void>(
            manager.join_room(
                SessionId{
                    std::string_view{
                        "session-42"}},
                roomId));
      }

      return room;
    }

    template <typename ManagerType>
    [[nodiscard]] Room *find_room(
        ManagerType &manager,
        const RoomId &roomId)
    {
      decltype(auto) result =
          manager.find_room(
              roomId);

      return room_pointer(
          result);
    }

    template <typename ManagerType>
    [[nodiscard]] CommandResult execute_command(
        ManagerType &manager,
        const RoomCommand &command)
    {
      return manager.execute(
          command);
    }

    template <typename ManagerType>
    [[nodiscard]] bool enqueue_command(
        ManagerType &manager,
        const RoomCommand &command)
    {
      if constexpr (
          std::same_as<
              decltype(manager.enqueue(
                  command)),
              bool>)
      {
        return manager.enqueue(
            command);
      }
      else
      {
        manager.enqueue(
            command);

        return true;
      }
    }

    template <typename ValueType>
    [[nodiscard]] std::optional<CommandResult>
    normalize_processed_result(
        ValueType &&value)
    {
      using Value =
          std::remove_cvref_t<
              ValueType>;

      if constexpr (
          std::same_as<
              Value,
              CommandResult>)
      {
        return std::forward<ValueType>(
            value);
      }
      else if constexpr (
          std::same_as<
              Value,
              std::optional<CommandResult>>)
      {
        return std::forward<ValueType>(
            value);
      }
      else if constexpr (
          requires {
            value.has_value();
            *value;
          })
      {
        if (!value.has_value())
        {
          return std::nullopt;
        }

        return CommandResult{
            *value};
      }
      else
      {
        static_assert(
            dependentFalse<Value>,
            "Unsupported process_next result");
      }
    }

    template <typename ManagerType>
    [[nodiscard]] std::optional<CommandResult>
    process_next_command(
        ManagerType &manager,
        const RoomId &roomId)
    {
      if constexpr (
          requires {
            manager.process_next(
                roomId);
          })
      {
        return normalize_processed_result(
            manager.process_next(
                roomId));
      }
      else if constexpr (
          requires {
            manager.process_next();
          })
      {
        return normalize_processed_result(
            manager.process_next());
      }
      else
      {
        static_assert(
            dependentFalse<ManagerType>,
            "Unsupported RoomManager process_next API");
      }
    }

    struct ManagerFixture
    {
      Config config{};

      std::shared_ptr<FactoryProbe>
          probe{
              std::make_shared<
                  FactoryProbe>()};

      std::unique_ptr<RoomManager>
          manager{};
    };

    [[nodiscard]] RoomId make_room_id(
        std::string_view value =
            "counter/main")
    {
      return RoomId{
          value};
    }

    [[nodiscard]] SessionId make_session_id(
        std::string_view value =
            "session-42")
    {
      return SessionId{
          value};
    }

    [[nodiscard]] RoomCommand make_command(
        RoomId roomId,
        std::string type,
        std::int64_t amount,
        std::string requestId)
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      RoomCommand command{
          std::move(roomId),
          make_session_id(),
          std::move(type),
          std::move(payload),
          std::move(requestId)};

      command.set_correlation_id(
          CorrelationId{
              "correlation-42"});

      return command;
    }

    [[nodiscard]] ManagerFixture
    make_fixture(
        std::size_t maxSessionsPerRoom = 16)
    {
      ManagerFixture fixture;

      fixture.config.maxActiveRooms = 16;
      fixture.config.maxPendingCommandsPerRoom = 16;
      fixture.config.maxSessionsPerRoom = maxSessionsPerRoom;
      fixture.config.snapshotEveryEvents = 0;
      fixture.config.snapshotOnRoomClose = false;

      fixture.manager =
          std::make_unique<RoomManager>(
              NodeId{
                  std::string_view{
                      "node-1"}},
              fixture.config);

      const SharedCounterFactory sharedFactory{
          fixture.manager->event_store(),
          fixture.manager->snapshot_store(),
          fixture.config,
          fixture.probe};

      const UniqueCounterFactory uniqueFactory{
          fixture.manager->event_store(),
          fixture.manager->snapshot_store(),
          fixture.config,
          fixture.probe};

      register_counter_factory(
          *fixture.manager,
          "counter",
          sharedFactory,
          uniqueFactory);

      static_cast<void>(
          fixture.manager->create_session(
              make_session_id()));

      return fixture;
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

    [[nodiscard]] bool execution_is_rejected(
        RoomManager &manager,
        const RoomCommand &command)
    {
      try
      {
        return execute_command(
                   manager,
                   command)
            .is_rejected();
      }
      catch (const Error &)
      {
        return true;
      }
    }

    TEST(RoomManagerRoutingTest, RoutesCommandToOpenedRoom)
    {
      ManagerFixture fixture =
          make_fixture();

      Room *room =
          open_room(
              *fixture.manager,
              make_room_id(),
              "counter");

      ASSERT_NE(
          room,
          nullptr);

      const CommandResult result =
          execute_command(
              *fixture.manager,
              make_command(
                  make_room_id(),
                  "counter.increment",
                  5,
                  "request-1"));

      EXPECT_TRUE(
          result.is_accepted());

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{5});
    }

    TEST(RoomManagerRoutingTest, JoiningDetachedSessionCreatesDetachedPresence)
    {
      ManagerFixture fixture =
          make_fixture();

      ASSERT_NE(
          open_room(
              *fixture.manager,
              make_room_id(),
              "counter"),
          nullptr);

      const auto presence =
          fixture.manager->find_presence(
              make_room_id(),
              make_session_id());

      ASSERT_TRUE(presence.has_value());
      EXPECT_EQ(presence->status(), PresenceStatus::Detached);
      EXPECT_TRUE(presence->connection_id().empty());
      EXPECT_TRUE(presence->detached_at().has_value());
    }

    TEST(RoomManagerRoutingTest, FailedJoinRollsBackPresence)
    {
      ManagerFixture fixture =
          make_fixture(1);

      ASSERT_NE(
          open_room(
              *fixture.manager,
              make_room_id(),
              "counter"),
          nullptr);

      const SessionId secondSessionId{
          std::string_view{
              "session-43"}};
      static_cast<void>(
          fixture.manager->create_session(secondSessionId));

      EXPECT_THROW(
          static_cast<void>(
              fixture.manager->join_room(
                  secondSessionId,
                  make_room_id())),
          Error);

      EXPECT_FALSE(
          fixture.manager
              ->find_presence(
                  make_room_id(),
                  secondSessionId)
              .has_value());
      EXPECT_FALSE(
          fixture.manager
              ->require_session(secondSessionId)
              ->has_room(make_room_id()));
    }

    TEST(RoomManagerRoutingTest, RejectsCommandForUnknownSession)
    {
      ManagerFixture fixture =
          make_fixture();

      ASSERT_NE(
          open_room(
              *fixture.manager,
              make_room_id(),
              "counter"),
          nullptr);

      JsonObject payload;
      payload.set_i64("amount", 1);

      const RoomCommand command{
          make_room_id(),
          SessionId{
              std::string_view{
                  "session-missing"}},
          "counter.increment",
          payload,
          "request-1"};

      try
      {
        static_cast<void>(
            fixture.manager->execute(command));
        FAIL() << "expected an unknown session error";
      }
      catch (const Error &error)
      {
        EXPECT_EQ(error.code(), ErrorCode::SessionNotFound);
      }
    }

    TEST(RoomManagerRoutingTest,
         ServerAndManagerPrioritizeUnknownSessionConsistently)
    {
      auto manager =
          std::make_shared<RoomManager>(
              NodeId{
                  std::string_view{
                      "node-1"}},
              Config{});

      Server server{manager};
      ASSERT_TRUE(server.start());

      const RoomCommand command =
          make_command(
              make_room_id("counter/missing"),
              "counter.increment",
              1,
              "request-1");

      try
      {
        static_cast<void>(manager->execute(command));
        FAIL() << "expected an unknown session error";
      }
      catch (const Error &error)
      {
        EXPECT_EQ(error.code(), ErrorCode::SessionNotFound);
      }

      try
      {
        static_cast<void>(server.execute(command));
        FAIL() << "expected an unknown session error";
      }
      catch (const Error &error)
      {
        EXPECT_EQ(error.code(), ErrorCode::SessionNotFound);
      }
    }

    TEST(RoomManagerRoutingTest, RejectsCommandForExistingSessionOutsideRoom)
    {
      ManagerFixture fixture =
          make_fixture();

      ASSERT_NE(
          open_room(
              *fixture.manager,
              make_room_id(),
              "counter"),
          nullptr);

      static_cast<void>(
          fixture.manager->create_session(
              SessionId{
                  std::string_view{
                      "session-outside"}}));

      JsonObject payload;
      payload.set_i64("amount", 1);

      const RoomCommand command{
          make_room_id(),
          SessionId{
              std::string_view{
                  "session-outside"}},
          "counter.increment",
          payload,
          "request-1"};

      try
      {
        static_cast<void>(
            fixture.manager->execute(command));
        FAIL() << "expected a missing membership error";
      }
      catch (const Error &error)
      {
        EXPECT_EQ(error.code(), ErrorCode::MembershipNotFound);
      }
    }

    TEST(RoomManagerRoutingTest, RejectsCommandForClosedSession)
    {
      ManagerFixture fixture =
          make_fixture();

      ASSERT_NE(
          open_room(
              *fixture.manager,
              make_room_id(),
              "counter"),
          nullptr);

      static_cast<void>(
          fixture.manager
              ->require_session(
                  make_session_id())
              ->close());

      try
      {
        static_cast<void>(
            fixture.manager->execute(
                make_command(
                    make_room_id(),
                    "counter.increment",
                    1,
                    "request-1")));
        FAIL() << "expected an expired session error";
      }
      catch (const Error &error)
      {
        EXPECT_EQ(error.code(), ErrorCode::SessionExpired);
      }
    }

    TEST(RoomManagerRoutingTest, ReturnsEventsProducedByRoom)
    {
      ManagerFixture fixture =
          make_fixture();

      open_room(
          *fixture.manager,
          make_room_id(),
          "counter");

      const CommandResult result =
          execute_command(
              *fixture.manager,
              make_command(
                  make_room_id(),
                  "counter.increment",
                  5,
                  "request-1"));

      ASSERT_TRUE(
          result.is_accepted());

      ASSERT_EQ(
          result.events().size(),
          1U);

      EXPECT_EQ(
          result.events()
              .front()
              .room_id(),
          make_room_id());

      EXPECT_EQ(
          result.events()
              .front()
              .type(),
          "counter.incremented");

      EXPECT_EQ(
          result.events()
              .front()
              .event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          result.events()
              .front()
              .room_version()
              .value(),
          VersionValue{1});
    }

    TEST(RoomManagerRoutingTest, RoutesCommandsToIndependentRooms)
    {
      ManagerFixture fixture =
          make_fixture();

      const RoomId firstId =
          make_room_id(
              "counter/first");

      const RoomId secondId =
          make_room_id(
              "counter/second");

      Room *first =
          open_room(
              *fixture.manager,
              firstId,
              "counter");

      Room *second =
          open_room(
              *fixture.manager,
              secondId,
              "counter");

      ASSERT_NE(
          first,
          nullptr);

      ASSERT_NE(
          second,
          nullptr);

      EXPECT_TRUE(
          execute_command(
              *fixture.manager,
              make_command(
                  firstId,
                  "counter.increment",
                  5,
                  "request-1"))
              .is_accepted());

      EXPECT_TRUE(
          execute_command(
              *fixture.manager,
              make_command(
                  secondId,
                  "counter.increment",
                  9,
                  "request-2"))
              .is_accepted());

      EXPECT_EQ(
          counter_state(
              *first)
              .value(),
          std::int64_t{5});

      EXPECT_EQ(
          counter_state(
              *second)
              .value(),
          std::int64_t{9});
    }

    TEST(RoomManagerRoutingTest, SequentialCommandsUseLatestRoomState)
    {
      ManagerFixture fixture =
          make_fixture();

      Room *room =
          open_room(
              *fixture.manager,
              make_room_id(),
              "counter");

      ASSERT_NE(
          room,
          nullptr);

      EXPECT_TRUE(
          execute_command(
              *fixture.manager,
              make_command(
                  make_room_id(),
                  "counter.increment",
                  10,
                  "request-1"))
              .is_accepted());

      EXPECT_TRUE(
          execute_command(
              *fixture.manager,
              make_command(
                  make_room_id(),
                  "counter.decrement",
                  3,
                  "request-2"))
              .is_accepted());

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{7});

      EXPECT_EQ(
          room->version()
              .value(),
          VersionValue{2});

      EXPECT_EQ(
          room->last_event_id()
              .value(),
          EventIdValue{2});
    }

    TEST(RoomManagerRoutingTest, BusinessRejectionDoesNotFailRoom)
    {
      ManagerFixture fixture =
          make_fixture();

      Room *room =
          open_room(
              *fixture.manager,
              make_room_id(),
              "counter");

      ASSERT_NE(
          room,
          nullptr);

      const CommandResult result =
          execute_command(
              *fixture.manager,
              make_command(
                  make_room_id(),
                  "counter.reject",
                  1,
                  "request-1"));

      EXPECT_TRUE(
          result.is_rejected());

      EXPECT_EQ(
          result.error_code(),
          ErrorCode::CommandRejected);

      EXPECT_TRUE(
          room->is_open());

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{0});
    }

    TEST(RoomManagerRoutingTest, RejectsCommandForUnknownRoom)
    {
      ManagerFixture fixture =
          make_fixture();

      EXPECT_TRUE(
          execution_is_rejected(
              *fixture.manager,
              make_command(
                  make_room_id(
                      "counter/missing"),
                  "counter.increment",
                  5,
                  "request-1")));

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);
    }

    TEST(RoomManagerRoutingTest, EnqueuesAndProcessesCommand)
    {
      ManagerFixture fixture =
          make_fixture();

      Room *room =
          open_room(
              *fixture.manager,
              make_room_id(),
              "counter");

      ASSERT_NE(
          room,
          nullptr);

      EXPECT_TRUE(
          enqueue_command(
              *fixture.manager,
              make_command(
                  make_room_id(),
                  "counter.increment",
                  5,
                  "request-1")));

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{0});

      const auto processed =
          process_next_command(
              *fixture.manager,
              make_room_id());

      ASSERT_TRUE(
          processed.has_value());

      EXPECT_TRUE(
          processed->is_accepted());

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{5});
    }

    TEST(RoomManagerRoutingTest, ProcessesQueuedCommandsInOrder)
    {
      ManagerFixture fixture =
          make_fixture();

      Room *room =
          open_room(
              *fixture.manager,
              make_room_id(),
              "counter");

      ASSERT_NE(
          room,
          nullptr);

      ASSERT_TRUE(
          enqueue_command(
              *fixture.manager,
              make_command(
                  make_room_id(),
                  "counter.increment",
                  10,
                  "request-1")));

      ASSERT_TRUE(
          enqueue_command(
              *fixture.manager,
              make_command(
                  make_room_id(),
                  "counter.decrement",
                  3,
                  "request-2")));

      const auto first =
          process_next_command(
              *fixture.manager,
              make_room_id());

      const auto second =
          process_next_command(
              *fixture.manager,
              make_room_id());

      ASSERT_TRUE(
          first.has_value());

      ASSERT_TRUE(
          second.has_value());

      EXPECT_TRUE(
          first->is_accepted());

      EXPECT_TRUE(
          second->is_accepted());

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{7});

      EXPECT_EQ(
          room->version()
              .value(),
          VersionValue{2});
    }

    TEST(RoomManagerRoutingTest, FindRoomReturnsRoutedRoom)
    {
      ManagerFixture fixture =
          make_fixture();

      Room *opened =
          open_room(
              *fixture.manager,
              make_room_id(),
              "counter");

      ASSERT_NE(
          opened,
          nullptr);

      Room *found =
          find_room(
              *fixture.manager,
              make_room_id());

      ASSERT_NE(
          found,
          nullptr);

      EXPECT_EQ(
          found,
          opened);
    }

  } // namespace

} // namespace vix::realtime
