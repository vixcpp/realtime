/**
 *
 * @file room_manager_cleanup_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for inactive room cleanup in the Vix Realtime room manager.
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
#include <thread>
#include <type_traits>
#include <utility>

#include <vix/json/json.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_store.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_manager.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/snapshot_store.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    template <typename>
    inline constexpr bool dependentFalse = false;

    template <typename Duration>
    [[nodiscard]] Duration convert_timeout(
        std::chrono::milliseconds value)
    {
      if constexpr (
          requires {
            typename Duration::rep;
            typename Duration::period;
          })
      {
        return std::chrono::duration_cast<
            Duration>(
            value);
      }
      else
      {
        return static_cast<Duration>(
            value.count());
      }
    }

    template <typename ConfigType>
    void set_room_idle_timeout(
        ConfigType &config,
        std::chrono::milliseconds timeout)
    {
      if constexpr (
          requires {
            config.roomIdleTimeout;
          })
      {
        using Timeout =
            std::remove_cvref_t<
                decltype(config.roomIdleTimeout)>;

        config.roomIdleTimeout =
            convert_timeout<Timeout>(
                timeout);
      }
      else if constexpr (
          requires {
            config.inactiveRoomTimeout;
          })
      {
        using Timeout =
            std::remove_cvref_t<
                decltype(config.inactiveRoomTimeout)>;

        config.inactiveRoomTimeout =
            convert_timeout<Timeout>(
                timeout);
      }
      else if constexpr (
          requires {
            config.roomInactivityTimeout;
          })
      {
        using Timeout =
            std::remove_cvref_t<
                decltype(config.roomInactivityTimeout)>;

        config.roomInactivityTimeout =
            convert_timeout<Timeout>(
                timeout);
      }
      else if constexpr (
          requires {
            config.idleRoomTimeout;
          })
      {
        using Timeout =
            std::remove_cvref_t<
                decltype(config.idleRoomTimeout)>;

        config.idleRoomTimeout =
            convert_timeout<Timeout>(
                timeout);
      }
      else if constexpr (
          requires {
            config.roomTimeout;
          })
      {
        using Timeout =
            std::remove_cvref_t<
                decltype(config.roomTimeout)>;

        config.roomTimeout =
            convert_timeout<Timeout>(
                timeout);
      }
      else if constexpr (
          requires {
            config.roomIdleTimeoutMs;
          })
      {
        config.roomIdleTimeoutMs =
            static_cast<
                decltype(config.roomIdleTimeoutMs)>(
                timeout.count());
      }
      else
      {
        static_assert(
            dependentFalse<ConfigType>,
            "Unsupported inactive room timeout configuration");
      }
    }

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
        if (event.type() !=
            "counter.incremented")
        {
          return;
        }

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
            "counter.increment")
        {
          return CommandResult::rejected(
              ErrorCode::InvalidCommand,
              "unsupported command");
        }

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

    struct FactoryProbe
    {
      std::size_t callCount{0};
    };

    class SharedRoomFactory
    {
    public:
      SharedRoomFactory(
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

    class UniqueRoomFactory
    {
    public:
      UniqueRoomFactory(
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
    void register_room_factory(
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
            "RoomManager must expose an owning room handle");
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
            "Unsupported RoomManager open_room API");
      }
    }

    template <typename ManagerType>
    [[nodiscard]] std::size_t cleanup_rooms(
        ManagerType &manager,
        Timestamp now)
    {
      const std::size_t before =
          manager.room_count();

      if constexpr (
          requires {
            {
              manager.cleanup_inactive(
                  now)
            } -> std::convertible_to<
                std::size_t>;
          })
      {
        return static_cast<std::size_t>(
            manager.cleanup_inactive(
                now));
      }
      else if constexpr (
          requires {
            manager.cleanup_inactive(
                now);
          })
      {
        manager.cleanup_inactive(
            now);
      }
      else if constexpr (
          requires {
            {
              manager.cleanup(
                  now)
            } -> std::convertible_to<
                std::size_t>;
          })
      {
        return static_cast<std::size_t>(
            manager.cleanup(
                now));
      }
      else if constexpr (
          requires {
            manager.cleanup(
                now);
          })
      {
        manager.cleanup(
            now);
      }
      else if constexpr (
          requires {
            {
              manager.prune_inactive(
                  now)
            } -> std::convertible_to<
                std::size_t>;
          })
      {
        return static_cast<std::size_t>(
            manager.prune_inactive(
                now));
      }
      else if constexpr (
          requires {
            manager.prune_inactive(
                now);
          })
      {
        manager.prune_inactive(
            now);
      }
      else if constexpr (
          requires {
            {
              manager.cleanup_inactive()
            } -> std::convertible_to<
                std::size_t>;
          })
      {
        return static_cast<std::size_t>(
            manager.cleanup_inactive());
      }
      else if constexpr (
          requires {
            manager.cleanup_inactive();
          })
      {
        manager.cleanup_inactive();
      }
      else if constexpr (
          requires {
            {
              manager.cleanup()
            } -> std::convertible_to<
                std::size_t>;
          })
      {
        return static_cast<std::size_t>(
            manager.cleanup());
      }
      else if constexpr (
          requires {
            manager.cleanup();
          })
      {
        manager.cleanup();
      }
      else
      {
        static_assert(
            dependentFalse<ManagerType>,
            "Unsupported RoomManager cleanup API");
      }

      const std::size_t after =
          manager.room_count();

      return before >= after
                 ? before - after
                 : 0U;
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

    [[nodiscard]] SessionId make_session_id()
    {
      return SessionId{
          std::string_view{
              "session-42"}};
    }

    [[nodiscard]] RoomCommand make_command(
        const RoomId &roomId,
        std::int64_t amount)
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      RoomCommand command{
          roomId,
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

    [[nodiscard]] ManagerFixture make_fixture(
        std::chrono::milliseconds idleTimeout,
        bool snapshotOnClose = false,
        std::size_t maximumActiveRooms = 16)
    {
      ManagerFixture fixture;

      fixture.config.maxActiveRooms =
          maximumActiveRooms;

      fixture.config.snapshotEveryEvents = 0;
      fixture.config.snapshotOnRoomClose =
          snapshotOnClose;

      fixture.config.snapshotsToKeep = 3;

      set_room_idle_timeout(
          fixture.config,
          idleTimeout);

      fixture.manager =
          std::make_unique<RoomManager>(
              NodeId{
                  std::string_view{
                      "node-1"}},
              fixture.config);

      const SharedRoomFactory sharedFactory{
          fixture.manager->event_store(),
          fixture.manager->snapshot_store(),
          fixture.config,
          fixture.probe};

      const UniqueRoomFactory uniqueFactory{
          fixture.manager->event_store(),
          fixture.manager->snapshot_store(),
          fixture.config,
          fixture.probe};

      register_room_factory(
          *fixture.manager,
          "counter",
          sharedFactory,
          uniqueFactory);

      return fixture;
    }

    TEST(RoomManagerCleanupTest, EmptyManagerCleansNothing)
    {
      ManagerFixture fixture =
          make_fixture(
              std::chrono::seconds{
                  30});

      const std::size_t removed =
          cleanup_rooms(
              *fixture.manager,
              SystemClock::now() +
                  std::chrono::minutes{
                      1});

      EXPECT_EQ(
          removed,
          0U);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);
    }

    TEST(RoomManagerCleanupTest, KeepsRoomBeforeIdleTimeout)
    {
      ManagerFixture fixture =
          make_fixture(
              std::chrono::hours{
                  1});

      const RoomId roomId =
          make_room_id();

      const auto room =
          open_room(
              *fixture.manager,
              roomId);

      ASSERT_NE(
          room,
          nullptr);

      const std::size_t removed =
          cleanup_rooms(
              *fixture.manager,
              SystemClock::now());

      EXPECT_EQ(
          removed,
          0U);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          1U);

      EXPECT_TRUE(
          fixture.manager
              ->has_room(
                  roomId));

      EXPECT_TRUE(
          room->is_open());
    }

    TEST(RoomManagerCleanupTest, RemovesIdleRoom)
    {
      ManagerFixture fixture =
          make_fixture(
              std::chrono::milliseconds{
                  1});

      const RoomId roomId =
          make_room_id();

      const auto room =
          open_room(
              *fixture.manager,
              roomId);

      ASSERT_NE(
          room,
          nullptr);

      std::this_thread::sleep_for(
          std::chrono::milliseconds{
              5});

      const std::size_t removed =
          cleanup_rooms(
              *fixture.manager,
              SystemClock::now() +
                  std::chrono::seconds{
                      10});

      EXPECT_EQ(
          removed,
          1U);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);

      EXPECT_FALSE(
          fixture.manager
              ->has_room(
                  roomId));
    }

    TEST(RoomManagerCleanupTest, ClosesRemovedRoom)
    {
      ManagerFixture fixture =
          make_fixture(
              std::chrono::milliseconds{
                  1});

      const auto room =
          open_room(
              *fixture.manager,
              make_room_id());

      ASSERT_NE(
          room,
          nullptr);

      ASSERT_TRUE(
          room->is_open());

      std::this_thread::sleep_for(
          std::chrono::milliseconds{
              5});

      cleanup_rooms(
          *fixture.manager,
          SystemClock::now() +
              std::chrono::seconds{
                  10});

      EXPECT_TRUE(
          room->is_closed());

      EXPECT_FALSE(
          room->is_open());

      EXPECT_EQ(
          room->status(),
          RoomStatus::Closed);
    }

    TEST(RoomManagerCleanupTest, RemovesMultipleIdleRooms)
    {
      ManagerFixture fixture =
          make_fixture(
              std::chrono::milliseconds{
                  1});

      ASSERT_NE(
          open_room(
              *fixture.manager,
              make_room_id(
                  "counter/first")),
          nullptr);

      ASSERT_NE(
          open_room(
              *fixture.manager,
              make_room_id(
                  "counter/second")),
          nullptr);

      ASSERT_NE(
          open_room(
              *fixture.manager,
              make_room_id(
                  "counter/third")),
          nullptr);

      ASSERT_EQ(
          fixture.manager
              ->room_count(),
          3U);

      std::this_thread::sleep_for(
          std::chrono::milliseconds{
              5});

      const std::size_t removed =
          cleanup_rooms(
              *fixture.manager,
              SystemClock::now() +
                  std::chrono::seconds{
                      10});

      EXPECT_EQ(
          removed,
          3U);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);

      EXPECT_TRUE(
          fixture.manager
              ->room_ids()
              .empty());
    }

    TEST(RoomManagerCleanupTest, CleanupIsIdempotent)
    {
      ManagerFixture fixture =
          make_fixture(
              std::chrono::milliseconds{
                  1});

      ASSERT_NE(
          open_room(
              *fixture.manager,
              make_room_id()),
          nullptr);

      std::this_thread::sleep_for(
          std::chrono::milliseconds{
              5});

      const Timestamp now =
          SystemClock::now() +
          std::chrono::seconds{
              10};

      EXPECT_EQ(
          cleanup_rooms(
              *fixture.manager,
              now),
          1U);

      EXPECT_EQ(
          cleanup_rooms(
              *fixture.manager,
              now),
          0U);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);
    }

    TEST(RoomManagerCleanupTest, CleanupFreesActiveRoomCapacity)
    {
      ManagerFixture fixture =
          make_fixture(
              std::chrono::milliseconds{
                  1},
              false,
              1);

      const RoomId firstId =
          make_room_id(
              "counter/first");

      const RoomId secondId =
          make_room_id(
              "counter/second");

      ASSERT_NE(
          open_room(
              *fixture.manager,
              firstId),
          nullptr);

      std::this_thread::sleep_for(
          std::chrono::milliseconds{
              5});

      ASSERT_EQ(
          cleanup_rooms(
              *fixture.manager,
              SystemClock::now() +
                  std::chrono::seconds{
                      10}),
          1U);

      const auto second =
          open_room(
              *fixture.manager,
              secondId);

      ASSERT_NE(
          second,
          nullptr);

      EXPECT_TRUE(
          second->is_open());

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          1U);

      EXPECT_TRUE(
          fixture.manager
              ->has_room(
                  secondId));
    }

    TEST(RoomManagerCleanupTest, PersistsFinalSnapshotWhenConfigured)
    {
      ManagerFixture fixture =
          make_fixture(
              std::chrono::milliseconds{
                  1},
              true);

      const RoomId roomId =
          make_room_id();

      const auto room =
          open_room(
              *fixture.manager,
              roomId);

      ASSERT_NE(
          room,
          nullptr);

      static_cast<void>(
          fixture.manager->create_session(
              make_session_id(),
              Identity{
                  "user-42"}));

      ASSERT_TRUE(
          fixture.manager->join_room(
              make_session_id(),
              roomId)
              .is_accepted());

      const CommandResult result =
          fixture.manager->execute(
              make_command(
                  roomId,
                  5));

      ASSERT_TRUE(
          result.is_accepted());

      ASSERT_TRUE(
          fixture.manager->leave_room(
              make_session_id(),
              roomId)
              .is_accepted());

      std::this_thread::sleep_for(
          std::chrono::milliseconds{
              5});

      ASSERT_EQ(
          cleanup_rooms(
              *fixture.manager,
              SystemClock::now() +
                  std::chrono::seconds{
                      10}),
          1U);

      const auto snapshot =
          fixture.manager
              ->snapshot_store()
              ->load_latest(
                  roomId);

      ASSERT_TRUE(
          snapshot.has_value());

      EXPECT_EQ(
          snapshot->room_version()
              .value(),
          VersionValue{1});

      EXPECT_EQ(
          snapshot->last_event_id()
              .value(),
          EventIdValue{1});

      const auto state =
          vix::json::to_json(
              snapshot->state());

      EXPECT_EQ(
          state.at("value")
              .get<std::int64_t>(),
          std::int64_t{5});
    }

  } // namespace

} // namespace vix::realtime
