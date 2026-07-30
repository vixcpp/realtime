/**
 *
 * @file room_manager_limits_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for active room limits in the Vix Realtime room manager.
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
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/errors.hpp>
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
#include <vix/realtime/snapshot_store.hpp>
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

    struct FactoryProbe
    {
      std::size_t callCount{0};
      bool fail{false};
      std::vector<RoomId> createdRooms{};
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

        probe_->createdRooms.push_back(
            roomId);

        if (probe_->fail)
        {
          throw Error{
              ErrorCode::InternalError,
              "room factory failed"};
        }

        return std::make_shared<Room>(
            roomId,
            std::make_unique<EmptyState>(),
            std::make_unique<EmptyHandler>(),
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

        probe_->createdRooms.push_back(
            roomId);

        if (probe_->fail)
        {
          throw Error{
              ErrorCode::InternalError,
              "room factory failed"};
        }

        return std::make_unique<Room>(
            roomId,
            std::make_unique<EmptyState>(),
            std::make_unique<EmptyHandler>(),
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

        return room_pointer(
            result);
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

        return room_pointer(
            result);
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

        return room_pointer(
            result);
      }
      else
      {
        static_assert(
            dependentFalse<ManagerType>,
            "Unsupported RoomManager open_room API");
      }
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
    void close_room(
        ManagerType &manager,
        const RoomId &roomId)
    {
      static_cast<void>(
          manager.close_room(
              roomId));
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
        std::string_view value)
    {
      return RoomId{
          value};
    }

    [[nodiscard]] ManagerFixture
    make_fixture(
        std::size_t maximumActiveRooms)
    {
      ManagerFixture fixture;

      fixture.config.maxActiveRooms =
          maximumActiveRooms;

      fixture.config.snapshotEveryEvents = 0;
      fixture.config.snapshotOnRoomClose = false;

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
          "empty",
          sharedFactory,
          uniqueFactory);

      return fixture;
    }

    [[nodiscard]] bool opening_is_rejected(
        RoomManager &manager,
        const RoomId &roomId,
        std::string_view factoryType =
            "empty")
    {
      try
      {
        return open_room(
                   manager,
                   roomId,
                   factoryType) == nullptr;
      }
      catch (const Error &)
      {
        return true;
      }
    }

    TEST(RoomManagerLimitsTest, AllowsRoomsBelowConfiguredLimit)
    {
      ManagerFixture fixture =
          make_fixture(
              3);

      EXPECT_NE(
          open_room(
              *fixture.manager,
              make_room_id(
                  "room/first"),
              "empty"),
          nullptr);

      EXPECT_NE(
          open_room(
              *fixture.manager,
              make_room_id(
                  "room/second"),
              "empty"),
          nullptr);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          2U);

      EXPECT_EQ(
          fixture.probe->callCount,
          2U);
    }

    TEST(RoomManagerLimitsTest, AllowsRoomAtConfiguredLimit)
    {
      ManagerFixture fixture =
          make_fixture(
              2);

      EXPECT_NE(
          open_room(
              *fixture.manager,
              make_room_id(
                  "room/first"),
              "empty"),
          nullptr);

      EXPECT_NE(
          open_room(
              *fixture.manager,
              make_room_id(
                  "room/second"),
              "empty"),
          nullptr);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          2U);
    }

    TEST(RoomManagerLimitsTest, RejectsRoomAboveConfiguredLimit)
    {
      ManagerFixture fixture =
          make_fixture(
              2);

      const RoomId firstId =
          make_room_id(
              "room/first");

      const RoomId secondId =
          make_room_id(
              "room/second");

      const RoomId thirdId =
          make_room_id(
              "room/third");

      ASSERT_NE(
          open_room(
              *fixture.manager,
              firstId,
              "empty"),
          nullptr);

      ASSERT_NE(
          open_room(
              *fixture.manager,
              secondId,
              "empty"),
          nullptr);

      EXPECT_TRUE(
          opening_is_rejected(
              *fixture.manager,
              thirdId));

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          2U);

      EXPECT_EQ(
          find_room(
              *fixture.manager,
              thirdId),
          nullptr);
    }

    TEST(RoomManagerLimitsTest, RejectedRoomIsNotOwnedOrRegistered)
    {
      ManagerFixture fixture =
          make_fixture(
              1);

      const RoomId acceptedId =
          make_room_id(
              "room/accepted");

      const RoomId rejectedId =
          make_room_id(
              "room/rejected");

      ASSERT_NE(
          open_room(
              *fixture.manager,
              acceptedId,
              "empty"),
          nullptr);

      ASSERT_TRUE(
          opening_is_rejected(
              *fixture.manager,
              rejectedId));

      EXPECT_TRUE(
          fixture.manager
              ->has_room(
                  acceptedId));

      EXPECT_FALSE(
          fixture.manager
              ->has_room(
                  rejectedId));

      EXPECT_EQ(
          find_room(
              *fixture.manager,
              rejectedId),
          nullptr);
    }

    TEST(RoomManagerLimitsTest, OpeningExistingRoomDoesNotConsumeCapacity)
    {
      ManagerFixture fixture =
          make_fixture(
              1);

      const RoomId roomId =
          make_room_id(
              "room/main");

      Room *first =
          open_room(
              *fixture.manager,
              roomId,
              "empty");

      ASSERT_NE(
          first,
          nullptr);

      Room *second =
          open_room(
              *fixture.manager,
              roomId,
              "empty");

      ASSERT_NE(
          second,
          nullptr);

      EXPECT_EQ(
          second,
          first);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          1U);

      EXPECT_EQ(
          fixture.probe->callCount,
          1U);
    }

    TEST(RoomManagerLimitsTest, ClosingRoomFreesCapacity)
    {
      ManagerFixture fixture =
          make_fixture(
              1);

      const RoomId firstId =
          make_room_id(
              "room/first");

      const RoomId secondId =
          make_room_id(
              "room/second");

      ASSERT_NE(
          open_room(
              *fixture.manager,
              firstId,
              "empty"),
          nullptr);

      ASSERT_TRUE(
          opening_is_rejected(
              *fixture.manager,
              secondId));

      close_room(
          *fixture.manager,
          firstId);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);

      Room *second =
          open_room(
              *fixture.manager,
              secondId,
              "empty");

      ASSERT_NE(
          second,
          nullptr);

      EXPECT_TRUE(
          second->is_open());

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          1U);
    }

    TEST(RoomManagerLimitsTest, ClosedRoomIsRemovedFromDirectory)
    {
      ManagerFixture fixture =
          make_fixture(
              1);

      const RoomId roomId =
          make_room_id(
              "room/main");

      ASSERT_NE(
          open_room(
              *fixture.manager,
              roomId,
              "empty"),
          nullptr);

      ASSERT_TRUE(
          fixture.manager
              ->has_room(
                  roomId));

      close_room(
          *fixture.manager,
          roomId);

      EXPECT_FALSE(
          fixture.manager
              ->has_room(
                  roomId));

      EXPECT_EQ(
          find_room(
              *fixture.manager,
              roomId),
          nullptr);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);
    }

    TEST(RoomManagerLimitsTest, FailedFactoryDoesNotConsumeCapacity)
    {
      ManagerFixture fixture =
          make_fixture(
              1);

      const RoomId failedId =
          make_room_id(
              "room/failed");

      fixture.probe->fail = true;

      EXPECT_TRUE(
          opening_is_rejected(
              *fixture.manager,
              failedId));

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);

      EXPECT_FALSE(
          fixture.manager
              ->has_room(
                  failedId));

      fixture.probe->fail = false;

      const RoomId validId =
          make_room_id(
              "room/valid");

      EXPECT_NE(
          open_room(
              *fixture.manager,
              validId,
              "empty"),
          nullptr);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          1U);
    }

    TEST(RoomManagerLimitsTest, UnknownFactoryDoesNotConsumeCapacity)
    {
      ManagerFixture fixture =
          make_fixture(
              1);

      const RoomId invalidId =
          make_room_id(
              "room/invalid");

      EXPECT_TRUE(
          opening_is_rejected(
              *fixture.manager,
              invalidId,
              "missing-factory"));

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          0U);

      EXPECT_FALSE(
          fixture.manager
              ->has_room(
                  invalidId));

      const RoomId validId =
          make_room_id(
              "room/valid");

      EXPECT_NE(
          open_room(
              *fixture.manager,
              validId,
              "empty"),
          nullptr);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          1U);
    }

    TEST(RoomManagerLimitsTest, LimitAppliesAcrossFactoryInstances)
    {
      ManagerFixture fixture =
          make_fixture(
              2);

      const RoomId firstId =
          make_room_id(
              "room/first");

      const RoomId secondId =
          make_room_id(
              "room/second");

      const RoomId thirdId =
          make_room_id(
              "room/third");

      ASSERT_NE(
          open_room(
              *fixture.manager,
              firstId,
              "empty"),
          nullptr);

      ASSERT_NE(
          open_room(
              *fixture.manager,
              secondId,
              "empty"),
          nullptr);

      EXPECT_TRUE(
          opening_is_rejected(
              *fixture.manager,
              thirdId));

      EXPECT_EQ(
          fixture.manager
              ->room_ids()
              .size(),
          2U);

      EXPECT_EQ(
          fixture.manager
              ->room_count(),
          2U);
    }

  } // namespace

} // namespace vix::realtime
