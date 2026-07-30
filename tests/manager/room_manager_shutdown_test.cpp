/**
 *
 * @file room_manager_shutdown_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for shutting down the Vix Realtime room manager.
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
        std::int64_t amount,
        std::string requestId)
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
          std::move(requestId)};

      command.set_correlation_id(
          CorrelationId{
              "correlation-84"});

      return command;
    }

    [[nodiscard]] ManagerFixture make_fixture(
        bool snapshotOnClose = false)
    {
      ManagerFixture fixture;

      fixture.config.maxActiveRooms = 16;
      fixture.config.maxPendingCommandsPerRoom = 16;
      fixture.config.snapshotEveryEvents = 0;
      fixture.config.snapshotOnRoomClose =
          snapshotOnClose;

      fixture.config.snapshotsToKeep = 3;

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

    [[nodiscard]] bool execution_is_rejected(
        RoomManager &manager,
        const RoomCommand &command)
    {
      try
      {
        return manager.execute(
                          command)
            .is_rejected();
      }
      catch (const Error &)
      {
        return true;
      }
    }

    TEST(RoomManagerShutdownTest, ShutsDownEmptyManager)
    {
      ManagerFixture fixture =
          make_fixture();

      EXPECT_NO_THROW(
          shutdown_manager(
              *fixture.manager));

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);

      EXPECT_TRUE(
          fixture.manager
              ->room_ids()
              .empty());
    }

    TEST(RoomManagerShutdownTest, ClosesEveryActiveRoom)
    {
      ManagerFixture fixture =
          make_fixture();

      const auto first =
          open_room(
              *fixture.manager,
              make_room_id(
                  "counter/first"));

      const auto second =
          open_room(
              *fixture.manager,
              make_room_id(
                  "counter/second"));

      const auto third =
          open_room(
              *fixture.manager,
              make_room_id(
                  "counter/third"));

      ASSERT_NE(
          first,
          nullptr);

      ASSERT_NE(
          second,
          nullptr);

      ASSERT_NE(
          third,
          nullptr);

      ASSERT_TRUE(
          first->is_open());

      ASSERT_TRUE(
          second->is_open());

      ASSERT_TRUE(
          third->is_open());

      shutdown_manager(
          *fixture.manager);

      EXPECT_TRUE(
          first->is_closed());

      EXPECT_TRUE(
          second->is_closed());

      EXPECT_TRUE(
          third->is_closed());
    }

    TEST(RoomManagerShutdownTest, RemovesEveryRoomFromDirectory)
    {
      ManagerFixture fixture =
          make_fixture();

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

      ASSERT_NE(
          open_room(
              *fixture.manager,
              secondId),
          nullptr);

      ASSERT_EQ(
          fixture.manager
              ->room_count(),
          2U);

      shutdown_manager(
          *fixture.manager);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);

      EXPECT_TRUE(
          fixture.manager
              ->room_ids()
              .empty());

      EXPECT_FALSE(
          fixture.manager
              ->has_room(
                  firstId));

      EXPECT_FALSE(
          fixture.manager
              ->has_room(
                  secondId));
    }

    TEST(RoomManagerShutdownTest, PreservesPersistedEvents)
    {
      ManagerFixture fixture =
          make_fixture();

      const RoomId roomId =
          make_room_id();

      ASSERT_NE(
          open_room(
              *fixture.manager,
              roomId),
          nullptr);

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      roomId,
                      5,
                      "request-1"))
              .is_accepted());

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      roomId,
                      3,
                      "request-2"))
              .is_accepted());

      ASSERT_EQ(
          fixture.manager
              ->event_store()
              ->count(
                  roomId),
          2U);

      shutdown_manager(
          *fixture.manager);

      EXPECT_EQ(
          fixture.manager
              ->event_store()
              ->count(
                  roomId),
          2U);

      EXPECT_EQ(
          fixture.manager
              ->event_store()
              ->latest_event_id(
                  roomId)
              .value(),
          EventIdValue{2});
    }

    TEST(RoomManagerShutdownTest, CreatesFinalSnapshotsWhenEnabled)
    {
      ManagerFixture fixture =
          make_fixture(
              true);

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

      ASSERT_NE(
          open_room(
              *fixture.manager,
              secondId),
          nullptr);

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      firstId,
                      5,
                      "request-1"))
              .is_accepted());

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      secondId,
                      9,
                      "request-2"))
              .is_accepted());

      shutdown_manager(
          *fixture.manager);

      const auto firstSnapshot =
          fixture.manager
              ->snapshot_store()
              ->load_latest(
                  firstId);

      const auto secondSnapshot =
          fixture.manager
              ->snapshot_store()
              ->load_latest(
                  secondId);

      ASSERT_TRUE(
          firstSnapshot.has_value());

      ASSERT_TRUE(
          secondSnapshot.has_value());

      EXPECT_EQ(
          firstSnapshot
              ->room_version()
              .value(),
          VersionValue{1});

      EXPECT_EQ(
          secondSnapshot
              ->room_version()
              .value(),
          VersionValue{1});

      const auto firstState =
          vix::json::to_json(
              firstSnapshot->state());

      const auto secondState =
          vix::json::to_json(
              secondSnapshot->state());

      EXPECT_EQ(
          firstState.at("value")
              .get<std::int64_t>(),
          std::int64_t{5});

      EXPECT_EQ(
          secondState.at("value")
              .get<std::int64_t>(),
          std::int64_t{9});
    }

    TEST(RoomManagerShutdownTest, DoesNotSnapshotWhenDisabled)
    {
      ManagerFixture fixture =
          make_fixture(
              false);

      const RoomId roomId =
          make_room_id();

      ASSERT_NE(
          open_room(
              *fixture.manager,
              roomId),
          nullptr);

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      roomId,
                      5,
                      "request-1"))
              .is_accepted());

      shutdown_manager(
          *fixture.manager);

      EXPECT_FALSE(
          fixture.manager
              ->snapshot_store()
              ->load_latest(
                  roomId)
              .has_value());
    }

    TEST(RoomManagerShutdownTest, ShutdownIsIdempotent)
    {
      ManagerFixture fixture =
          make_fixture();

      const auto room =
          open_room(
              *fixture.manager,
              make_room_id());

      ASSERT_NE(
          room,
          nullptr);

      shutdown_manager(
          *fixture.manager);

      EXPECT_NO_THROW(
          shutdown_manager(
              *fixture.manager));

      EXPECT_TRUE(
          room->is_closed());

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);
    }

    TEST(RoomManagerShutdownTest, CommandsCannotReachRemovedRooms)
    {
      ManagerFixture fixture =
          make_fixture();

      const RoomId roomId =
          make_room_id();

      ASSERT_NE(
          open_room(
              *fixture.manager,
              roomId),
          nullptr);

      shutdown_manager(
          *fixture.manager);

      EXPECT_TRUE(
          execution_is_rejected(
              *fixture.manager,
              make_command(
                  roomId,
                  5,
                  "request-1")));

      EXPECT_EQ(
          fixture.manager
              ->event_store()
              ->count(
                  roomId),
          0U);
    }

    TEST(RoomManagerShutdownTest, PreservesFinalRoomPosition)
    {
      ManagerFixture fixture =
          make_fixture();

      const RoomId roomId =
          make_room_id();

      const auto room =
          open_room(
              *fixture.manager,
              roomId);

      ASSERT_NE(
          room,
          nullptr);

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      roomId,
                      5,
                      "request-1"))
              .is_accepted());

      ASSERT_TRUE(
          fixture.manager
              ->execute(
                  make_command(
                      roomId,
                      3,
                      "request-2"))
              .is_accepted());

      const RoomVersion finalVersion =
          room->version();

      const EventId finalEventId =
          room->last_event_id();

      shutdown_manager(
          *fixture.manager);

      EXPECT_EQ(
          room->version(),
          finalVersion);

      EXPECT_EQ(
          room->last_event_id(),
          finalEventId);

      EXPECT_EQ(
          finalVersion.value(),
          VersionValue{2});

      EXPECT_EQ(
          finalEventId.value(),
          EventIdValue{2});
    }

  } // namespace

} // namespace vix::realtime
