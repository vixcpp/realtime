/**
 *
 * @file room_manager_constructor_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime room manager construction.
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
#include <string_view>
#include <type_traits>
#include <utility>

#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_manager.hpp>
#include <vix/realtime/room_state.hpp>
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
    };

    template <typename ConfigType>
    void set_maximum_active_rooms(
        ConfigType &config,
        std::size_t value)
    {
      if constexpr (
          requires {
            config.maxActiveRooms = value;
          })
      {
        config.maxActiveRooms =
            value;
      }
      else if constexpr (
          requires {
            config.maxRooms = value;
          })
      {
        config.maxRooms =
            value;
      }
      else if constexpr (
          requires {
            config.maximumActiveRooms =
                value;
          })
      {
        config.maximumActiveRooms =
            value;
      }
      else
      {
        static_assert(
            dependentFalse<ConfigType>,
            "Unsupported active room configuration field");
      }
    }

    template <typename ConfigType>
    [[nodiscard]] std::size_t
    maximum_active_rooms(
        const ConfigType &config)
    {
      if constexpr (
          requires {
            config.maxActiveRooms;
          })
      {
        return static_cast<std::size_t>(
            config.maxActiveRooms);
      }
      else if constexpr (
          requires {
            config.maxRooms;
          })
      {
        return static_cast<std::size_t>(
            config.maxRooms);
      }
      else if constexpr (
          requires {
            config.maximumActiveRooms;
          })
      {
        return static_cast<std::size_t>(
            config.maximumActiveRooms);
      }
      else
      {
        static_assert(
            dependentFalse<ConfigType>,
            "Unsupported active room configuration field");
      }
    }

    template <
        typename ManagerType,
        typename SharedFactory,
        typename UniqueFactory>
    [[nodiscard]] std::unique_ptr<ManagerType>
    construct_manager(
        const Config &config,
        const NodeId &nodeId,
        const std::shared_ptr<MemoryEventStore>
            &eventStore,
        const std::shared_ptr<MemorySnapshotStore>
            &snapshotStore,
        const SharedFactory &sharedFactory,
        const UniqueFactory &uniqueFactory)
    {
      if constexpr (
          std::constructible_from<
              ManagerType,
              NodeId,
              SharedFactory,
              Config>)
      {
        return std::make_unique<
            ManagerType>(
            nodeId,
            sharedFactory,
            config);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              NodeId,
              UniqueFactory,
              Config>)
      {
        return std::make_unique<
            ManagerType>(
            nodeId,
            uniqueFactory,
            config);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              Config,
              NodeId,
              SharedFactory>)
      {
        return std::make_unique<
            ManagerType>(
            config,
            nodeId,
            sharedFactory);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              Config,
              NodeId,
              UniqueFactory>)
      {
        return std::make_unique<
            ManagerType>(
            config,
            nodeId,
            uniqueFactory);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              NodeId,
              Config,
              SharedFactory>)
      {
        return std::make_unique<
            ManagerType>(
            nodeId,
            config,
            sharedFactory);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              NodeId,
              Config,
              UniqueFactory>)
      {
        return std::make_unique<
            ManagerType>(
            nodeId,
            config,
            uniqueFactory);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              Config,
              SharedFactory,
              NodeId>)
      {
        return std::make_unique<
            ManagerType>(
            config,
            sharedFactory,
            nodeId);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              Config,
              UniqueFactory,
              NodeId>)
      {
        return std::make_unique<
            ManagerType>(
            config,
            uniqueFactory,
            nodeId);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              SharedFactory,
              NodeId,
              Config>)
      {
        return std::make_unique<
            ManagerType>(
            sharedFactory,
            nodeId,
            config);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              UniqueFactory,
              NodeId,
              Config>)
      {
        return std::make_unique<
            ManagerType>(
            uniqueFactory,
            nodeId,
            config);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              Config,
              NodeId,
              std::shared_ptr<
                  MemoryEventStore>,
              std::shared_ptr<
                  MemorySnapshotStore>,
              SharedFactory>)
      {
        return std::make_unique<
            ManagerType>(
            config,
            nodeId,
            eventStore,
            snapshotStore,
            sharedFactory);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              Config,
              NodeId,
              std::shared_ptr<
                  MemoryEventStore>,
              std::shared_ptr<
                  MemorySnapshotStore>,
              UniqueFactory>)
      {
        return std::make_unique<
            ManagerType>(
            config,
            nodeId,
            eventStore,
            snapshotStore,
            uniqueFactory);
      }
      else if constexpr (
          std::constructible_from<
              ManagerType,
              Config,
              NodeId,
              SharedFactory,
              std::shared_ptr<
                  MemoryEventStore>,
              std::shared_ptr<
                  MemorySnapshotStore>>)
      {
        return std::make_unique<
            ManagerType>(
            config,
            nodeId,
            sharedFactory,
            eventStore,
            snapshotStore);
      }
      else if constexpr (
        std::constructible_from<
            ManagerType,
            Config,
            NodeId,
            UniqueFactory,
            std::shared_ptr<
                MemoryEventStore>,
            std::shared_ptr<
                MemorySnapshotStore>>)
    {
      return std::make_unique<
          ManagerType>(
          config,
          nodeId,
          uniqueFactory,
          eventStore,
          snapshotStore);
    }
    else if constexpr (
        std::constructible_from<
            ManagerType,
            NodeId,
            SharedFactory,
            std::shared_ptr<
                MemoryEventStore>,
            std::shared_ptr<
                MemorySnapshotStore>,
            Config>)
    {
      return std::make_unique<
          ManagerType>(
          nodeId,
          sharedFactory,
          eventStore,
          snapshotStore,
          config);
    }
    else if constexpr (
        std::constructible_from<
            ManagerType,
            NodeId,
            UniqueFactory,
            std::shared_ptr<
                MemoryEventStore>,
            std::shared_ptr<
                MemorySnapshotStore>,
            Config>)
    {
      return std::make_unique<
          ManagerType>(
          nodeId,
          uniqueFactory,
          eventStore,
          snapshotStore,
          config);
    }
    else if constexpr (
        std::constructible_from<
            ManagerType,
            NodeId,
            SharedFactory>)
    {
      return std::make_unique<
          ManagerType>(
          nodeId,
          sharedFactory);
    }
    else if constexpr (
        std::constructible_from<
            ManagerType,
            NodeId,
            UniqueFactory>)
    {
      return std::make_unique<
          ManagerType>(
          nodeId,
          uniqueFactory);
    }
    else if constexpr (
        std::constructible_from<
            ManagerType,
            Config,
            SharedFactory>)
    {
      return std::make_unique<
          ManagerType>(
          config,
          sharedFactory);
    }
    else if constexpr (
        std::constructible_from<
            ManagerType,
            Config,
            UniqueFactory>)
    {
      return std::make_unique<
          ManagerType>(
          config,
          uniqueFactory);
    }
    else
    {
      static_assert(
          dependentFalse<ManagerType>,
          "Unsupported RoomManager constructor API");
    }
  }

  template <typename ManagerType>
  [[nodiscard]] const Config &
  manager_config(
      const ManagerType &manager)
  {
    if constexpr (
        requires {
          manager.config();
        })
    {
      return manager.config();
    }
    else if constexpr (
        requires {
          manager.configuration();
        })
    {
      return manager.configuration();
    }
    else
    {
      static_assert(
          dependentFalse<ManagerType>,
          "Unsupported RoomManager configuration API");
    }
  }

  template <typename ManagerType>
  [[nodiscard]] NodeId manager_node_id(
      const ManagerType &manager)
  {
    if constexpr (
        requires {
          manager.node_id();
        })
    {
      return manager.node_id();
    }
    else if constexpr (
        requires {
          manager.local_node_id();
        })
    {
      return manager.local_node_id();
    }
    else if constexpr (
        requires {
          manager.owner().node_id();
        })
    {
      return manager.owner()
          .node_id();
    }
    else if constexpr (
        requires {
          manager.room_owner()
              .node_id();
        })
    {
      return manager.room_owner()
          .node_id();
    }
    else
    {
      static_assert(
          dependentFalse<ManagerType>,
          "Unsupported RoomManager node identifier API");
    }
  }

  template <typename ManagerType>
  [[nodiscard]] std::size_t
  active_room_count(
      const ManagerType &manager)
  {
    if constexpr (
        requires {
          manager.active_room_count();
        })
    {
      return static_cast<std::size_t>(
          manager.active_room_count());
    }
    else if constexpr (
        requires {
          manager.room_count();
        })
    {
      return static_cast<std::size_t>(
          manager.room_count());
    }
    else if constexpr (
        requires {
          manager.size();
        })
    {
      return static_cast<std::size_t>(
          manager.size());
    }
    else if constexpr (
        requires {
          manager.directory()
              .size();
        })
    {
      return static_cast<std::size_t>(
          manager.directory()
              .size());
    }
    else
    {
      static_assert(
          dependentFalse<ManagerType>,
          "Unsupported RoomManager room count API");
    }
  }

  template <typename ManagerType>
  [[nodiscard]] bool manager_empty(
      const ManagerType &manager)
  {
    if constexpr (
        requires {
          manager.empty();
        })
    {
      return manager.empty();
    }
    else
    {
      return active_room_count(
                 manager) == 0U;
    }
  }

  template <typename ValueType>
  [[nodiscard]] const Room *
  room_pointer(
      const ValueType &value)
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
      return &value;
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
          {
            value.get()
          } -> std::convertible_to<
              const Room *>;
        })
    {
      return value.get();
    }
    else if constexpr (
        requires {
          {
            value.operator->()
          } -> std::convertible_to<
              const Room *>;
        })
    {
      return value.operator->();
    }
    else if constexpr (
        requires {
          value.lock();
        })
    {
      const auto locked =
          value.lock();

      return locked.get();
    }
    else
    {
      static_assert(
          dependentFalse<Value>,
          "Unsupported managed room handle");
    }
  }

  template <typename ManagerType>
  [[nodiscard]] const Room *
  find_managed_room(
      const ManagerType &manager,
      const RoomId &roomId)
  {
    if constexpr (
        requires {
          manager.find(
              roomId);
        })
    {
      decltype(auto) result =
          manager.find(
              roomId);

      return room_pointer(
          result);
    }
    else if constexpr (
        requires {
          manager.find_room(
              roomId);
        })
    {
      decltype(auto) result =
          manager.find_room(
              roomId);

      return room_pointer(
          result);
    }
    else if constexpr (
        requires {
          manager.get(
              roomId);
        })
    {
      decltype(auto) result =
          manager.get(
              roomId);

      return room_pointer(
          result);
    }
    else if constexpr (
        requires {
          manager.directory()
              .find(
                  roomId);
        })
    {
      decltype(auto) result =
          manager.directory()
              .find(
                  roomId);

      return room_pointer(
          result);
    }
    else
    {
      static_assert(
          dependentFalse<ManagerType>,
          "Unsupported RoomManager lookup API");
    }
  }

  struct ManagerFixture
  {
    Config config{};
    NodeId nodeId{
        std::string_view{
            "node-1"}};

    std::shared_ptr<MemoryEventStore>
        eventStore{
            std::make_shared<
                MemoryEventStore>()};

    std::shared_ptr<MemorySnapshotStore>
        snapshotStore{
            std::make_shared<
                MemorySnapshotStore>()};

    std::shared_ptr<FactoryProbe>
        probe{
            std::make_shared<
                FactoryProbe>()};

    std::unique_ptr<RoomManager>
        manager{};
  };

  [[nodiscard]] ManagerFixture
  make_fixture()
  {
    ManagerFixture fixture;

    set_maximum_active_rooms(
        fixture.config,
        8);

    const auto sharedFactory =
        [probe = fixture.probe,
         eventStore =
             fixture.eventStore,
         snapshotStore =
             fixture.snapshotStore,
         config =
             fixture.config](
            const RoomId &roomId,
            auto &&...)
        -> std::shared_ptr<Room>
    {
      ++probe->callCount;

      return std::make_shared<Room>(
          roomId,
          std::make_unique<
              EmptyState>(),
          std::make_unique<
              EmptyHandler>(),
          eventStore,
          snapshotStore,
          config);
    };

    const auto uniqueFactory =
        [probe = fixture.probe,
         eventStore =
             fixture.eventStore,
         snapshotStore =
             fixture.snapshotStore,
         config =
             fixture.config](
            const RoomId &roomId,
            auto &&...)
        -> std::unique_ptr<Room>
    {
      ++probe->callCount;

      return std::make_unique<Room>(
          roomId,
          std::make_unique<
              EmptyState>(),
          std::make_unique<
              EmptyHandler>(),
          eventStore,
          snapshotStore,
          config);
    };

    fixture.manager =
        construct_manager<RoomManager>(
            fixture.config,
            fixture.nodeId,
            fixture.eventStore,
            fixture.snapshotStore,
            sharedFactory,
            uniqueFactory);

    return fixture;
  }

  TEST(RoomManagerConstructorTest, ConstructsManager)
  {
    const ManagerFixture fixture =
        make_fixture();

    ASSERT_NE(
        fixture.manager,
        nullptr);
  }

  TEST(RoomManagerConstructorTest, StartsWithoutActiveRooms)
  {
    const ManagerFixture fixture =
        make_fixture();

    EXPECT_EQ(
        active_room_count(
            *fixture.manager),
        0U);

    EXPECT_TRUE(
        manager_empty(
            *fixture.manager));
  }

  TEST(RoomManagerConstructorTest, DoesNotInvokeFactoryDuringConstruction)
  {
    const ManagerFixture fixture =
        make_fixture();

    EXPECT_EQ(
        fixture.probe->callCount,
        0U);
  }

  TEST(RoomManagerConstructorTest, StoresLocalNodeIdentifier)
  {
    const ManagerFixture fixture =
        make_fixture();

    EXPECT_EQ(
        manager_node_id(
            *fixture.manager),
        fixture.nodeId);
  }

  TEST(RoomManagerConstructorTest, StoresConfiguration)
  {
    const ManagerFixture fixture =
        make_fixture();

    EXPECT_EQ(
        maximum_active_rooms(
            manager_config(
                *fixture.manager)),
        8U);
  }

  TEST(RoomManagerConstructorTest, UnknownRoomIsNotActive)
  {
    const ManagerFixture fixture =
        make_fixture();

    EXPECT_EQ(
        find_managed_room(
            *fixture.manager,
            RoomId{
                std::string_view{
                    "room/missing"}}),
        nullptr);
  }

  TEST(RoomManagerConstructorTest, ConstructionDoesNotCreateStoredEvents)
  {
    const ManagerFixture fixture =
        make_fixture();

    EXPECT_EQ(
        fixture.eventStore->count(
            RoomId{
                std::string_view{
                    "room/main"}}),
        0U);
  }

  TEST(RoomManagerConstructorTest, ConstructionDoesNotCreateSnapshots)
  {
    const ManagerFixture fixture =
        make_fixture();

    EXPECT_EQ(
        fixture.snapshotStore->count(
            RoomId{
                std::string_view{
                    "room/main"}}),
        0U);
  }

} // namespace

} // namespace vix::realtime
