/**
 *
 * @file room_owner_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for local room ownership in Vix Realtime.
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
#include <utility>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_owner.hpp>

namespace vix::realtime
{
  namespace
  {
    template <typename>
    inline constexpr bool dependentFalse = false;

    template <typename OwnerType = RoomOwner>
    [[nodiscard]] std::unique_ptr<OwnerType>
    make_owner(
        NodeId nodeId)
    {
      if constexpr (
          std::constructible_from<
              OwnerType,
              NodeId>)
      {
        return std::make_unique<
            OwnerType>(
            std::move(nodeId));
      }
      else if constexpr (
          std::default_initializable<
              OwnerType> &&
          requires(
              OwnerType &owner,
              NodeId id) {
            owner.set_node_id(
                std::move(id));
          })
      {
        auto owner =
            std::make_unique<
                OwnerType>();

        owner->set_node_id(
            std::move(nodeId));

        return owner;
      }
      else if constexpr (
          std::default_initializable<
              OwnerType> &&
          requires(
              OwnerType &owner,
              NodeId id) {
            owner.set_local_node_id(
                std::move(id));
          })
      {
        auto owner =
            std::make_unique<
                OwnerType>();

        owner->set_local_node_id(
            std::move(nodeId));

        return owner;
      }
      else
      {
        static_assert(
            dependentFalse<OwnerType>,
            "Unsupported RoomOwner constructor API");
      }
    }

    template <typename OwnerType>
    [[nodiscard]] NodeId owner_node_id(
        const OwnerType &owner)
    {
      if constexpr (
          requires {
            owner.node_id();
          })
      {
        return owner.node_id();
      }
      else if constexpr (
          requires {
            owner.local_node_id();
          })
      {
        return owner.local_node_id();
      }
      else if constexpr (
          requires {
            owner.owner_node_id();
          })
      {
        return owner.owner_node_id();
      }
      else
      {
        static_assert(
            dependentFalse<OwnerType>,
            "Unsupported RoomOwner node identifier API");
      }
    }

    template <typename OwnerType>
    [[nodiscard]] bool acquire_room(
        OwnerType &owner,
        const RoomId &roomId)
    {
      if constexpr (
          requires {
            {
              owner.acquire(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.acquire(
            roomId);
      }
      else if constexpr (
          requires {
            owner.acquire(
                roomId);
          })
      {
        owner.acquire(
            roomId);

        return true;
      }
      else if constexpr (
          requires {
            {
              owner.claim(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.claim(
            roomId);
      }
      else if constexpr (
          requires {
            owner.claim(
                roomId);
          })
      {
        owner.claim(
            roomId);

        return true;
      }
      else if constexpr (
          requires {
            {
              owner.assign(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.assign(
            roomId);
      }
      else if constexpr (
          requires {
            {
              owner.add(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.add(
            roomId);
      }
      else
      {
        static_assert(
            dependentFalse<OwnerType>,
            "Unsupported RoomOwner acquire API");
      }
    }

    template <typename OwnerType>
    [[nodiscard]] bool release_room(
        OwnerType &owner,
        const RoomId &roomId)
    {
      if constexpr (
          requires {
            {
              owner.release(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.release(
            roomId);
      }
      else if constexpr (
          requires {
            owner.release(
                roomId);
          })
      {
        owner.release(
            roomId);

        return true;
      }
      else if constexpr (
          requires {
            {
              owner.release_room(
                  roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.release_room(
            roomId);
      }
      else if constexpr (
          requires {
            owner.release_room(
                roomId);
          })
      {
        owner.release_room(
            roomId);

        return true;
      }
      else if constexpr (
          requires {
            {
              owner.remove(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.remove(
            roomId);
      }
      else if constexpr (
          requires {
            {
              owner.erase(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.erase(
            roomId);
      }
      else
      {
        static_assert(
            dependentFalse<OwnerType>,
            "Unsupported RoomOwner release API");
      }
    }

    template <typename OwnerType>
    [[nodiscard]] bool owns_room(
        const OwnerType &owner,
        const RoomId &roomId)
    {
      if constexpr (
          requires {
            {
              owner.owns(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.owns(
            roomId);
      }
      else if constexpr (
          requires {
            {
              owner.owns_room(
                  roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.owns_room(
            roomId);
      }
      else if constexpr (
          requires {
            {
              owner.contains(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.contains(
            roomId);
      }
      else if constexpr (
          requires {
            {
              owner.has_room(
                  roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return owner.has_room(
            roomId);
      }
      else
      {
        static_assert(
            dependentFalse<OwnerType>,
            "Unsupported RoomOwner ownership lookup API");
      }
    }

    template <typename OwnerType>
    [[nodiscard]] std::size_t owned_room_count(
        const OwnerType &owner)
    {
      if constexpr (
          requires {
            {
              owner.room_count()
            } -> std::convertible_to<std::size_t>;
          })
      {
        return static_cast<std::size_t>(
            owner.room_count());
      }
      else if constexpr (
          requires {
            {
              owner.size()
            } -> std::convertible_to<std::size_t>;
          })
      {
        return static_cast<std::size_t>(
            owner.size());
      }
      else if constexpr (
          requires {
            {
              owner.count()
            } -> std::convertible_to<std::size_t>;
          })
      {
        return static_cast<std::size_t>(
            owner.count());
      }
      else if constexpr (
          requires {
            owner.rooms().size();
          })
      {
        return static_cast<std::size_t>(
            owner.rooms().size());
      }
      else
      {
        static_assert(
            dependentFalse<OwnerType>,
            "Unsupported RoomOwner count API");
      }
    }

    template <typename OwnerType>
    [[nodiscard]] bool owner_empty(
        const OwnerType &owner)
    {
      if constexpr (
          requires {
            {
              owner.empty()
            } -> std::convertible_to<bool>;
          })
      {
        return owner.empty();
      }
      else
      {
        return owned_room_count(
                   owner) == 0U;
      }
    }

    template <typename OwnerType>
    void clear_owner(
        OwnerType &owner)
    {
      if constexpr (
          requires {
            owner.clear();
          })
      {
        owner.clear();
      }
      else if constexpr (
          requires {
            owner.release_all();
          })
      {
        owner.release_all();
      }
      else if constexpr (
          requires {
            owner.clear_rooms();
          })
      {
        owner.clear_rooms();
      }
      else
      {
        static_assert(
            dependentFalse<OwnerType>,
            "Unsupported RoomOwner clear API");
      }
    }

    [[nodiscard]] NodeId make_node_id(
        std::string_view value =
            "node-1")
    {
      return NodeId{
          value};
    }

    [[nodiscard]] RoomId make_room_id(
        std::string_view value)
    {
      return RoomId{
          value};
    }

    [[nodiscard]] bool try_acquire_room(
        RoomOwner &owner,
        const RoomId &roomId)
    {
      try
      {
        return acquire_room(
            owner,
            roomId);
      }
      catch (const std::exception &)
      {
        return false;
      }
    }

    TEST(RoomOwnerTest, StoresLocalNodeIdentifier)
    {
      const auto owner =
          make_owner(
              make_node_id());

      ASSERT_NE(
          owner,
          nullptr);

      EXPECT_EQ(
          owner_node_id(
              *owner),
          make_node_id());
    }

    TEST(RoomOwnerTest, StartsWithoutOwnedRooms)
    {
      const auto owner =
          make_owner(
              make_node_id());

      EXPECT_TRUE(
          owner_empty(
              *owner));

      EXPECT_EQ(
          owned_room_count(
              *owner),
          0U);

      EXPECT_FALSE(
          owns_room(
              *owner,
              make_room_id(
                  "room/main")));
    }

    TEST(RoomOwnerTest, AcquiresRoom)
    {
      auto owner =
          make_owner(
              make_node_id());

      const RoomId roomId =
          make_room_id(
              "room/main");

      EXPECT_TRUE(
          acquire_room(
              *owner,
              roomId));

      EXPECT_TRUE(
          owns_room(
              *owner,
              roomId));

      EXPECT_EQ(
          owned_room_count(
              *owner),
          1U);

      EXPECT_FALSE(
          owner_empty(
              *owner));
    }

    TEST(RoomOwnerTest, DuplicateAcquisitionIsIdempotent)
    {
      auto owner =
          make_owner(
              make_node_id());

      const RoomId roomId =
          make_room_id(
              "room/main");

      ASSERT_TRUE(
          try_acquire_room(
              *owner,
              roomId));

      static_cast<void>(
          try_acquire_room(
              *owner,
              roomId));

      EXPECT_TRUE(
          owns_room(
              *owner,
              roomId));

      EXPECT_EQ(
          owned_room_count(
              *owner),
          1U);
    }

    TEST(RoomOwnerTest, AcquiresMultipleRooms)
    {
      auto owner =
          make_owner(
              make_node_id());

      const RoomId first =
          make_room_id(
              "room/first");

      const RoomId second =
          make_room_id(
              "room/second");

      const RoomId third =
          make_room_id(
              "room/third");

      EXPECT_TRUE(
          acquire_room(
              *owner,
              first));

      EXPECT_TRUE(
          acquire_room(
              *owner,
              second));

      EXPECT_TRUE(
          acquire_room(
              *owner,
              third));

      EXPECT_EQ(
          owned_room_count(
              *owner),
          3U);

      EXPECT_TRUE(
          owns_room(
              *owner,
              first));

      EXPECT_TRUE(
          owns_room(
              *owner,
              second));

      EXPECT_TRUE(
          owns_room(
              *owner,
              third));
    }

    TEST(RoomOwnerTest, ReleasesOwnedRoom)
    {
      auto owner =
          make_owner(
              make_node_id());

      const RoomId first =
          make_room_id(
              "room/first");

      const RoomId second =
          make_room_id(
              "room/second");

      acquire_room(
          *owner,
          first);

      acquire_room(
          *owner,
          second);

      EXPECT_TRUE(
          release_room(
              *owner,
              first));

      EXPECT_FALSE(
          owns_room(
              *owner,
              first));

      EXPECT_TRUE(
          owns_room(
              *owner,
              second));

      EXPECT_EQ(
          owned_room_count(
              *owner),
          1U);
    }

    TEST(RoomOwnerTest, ReleasingUnknownRoomDoesNotChangeOwnership)
    {
      auto owner =
          make_owner(
              make_node_id());

      const RoomId owned =
          make_room_id(
              "room/owned");

      acquire_room(
          *owner,
          owned);

      static_cast<void>(
          release_room(
              *owner,
              make_room_id(
                  "room/missing")));

      EXPECT_EQ(
          owned_room_count(
              *owner),
          1U);

      EXPECT_TRUE(
          owns_room(
              *owner,
              owned));
    }

    TEST(RoomOwnerTest, ReleasedRoomCanBeAcquiredAgain)
    {
      auto owner =
          make_owner(
              make_node_id());

      const RoomId roomId =
          make_room_id(
              "room/main");

      acquire_room(
          *owner,
          roomId);

      release_room(
          *owner,
          roomId);

      EXPECT_FALSE(
          owns_room(
              *owner,
              roomId));

      EXPECT_TRUE(
          acquire_room(
              *owner,
              roomId));

      EXPECT_TRUE(
          owns_room(
              *owner,
              roomId));

      EXPECT_EQ(
          owned_room_count(
              *owner),
          1U);
    }

    TEST(RoomOwnerTest, ClearsEveryOwnedRoom)
    {
      auto owner =
          make_owner(
              make_node_id());

      const RoomId first =
          make_room_id(
              "room/first");

      const RoomId second =
          make_room_id(
              "room/second");

      acquire_room(
          *owner,
          first);

      acquire_room(
          *owner,
          second);

      ASSERT_EQ(
          owned_room_count(
              *owner),
          2U);

      clear_owner(
          *owner);

      EXPECT_TRUE(
          owner_empty(
              *owner));

      EXPECT_EQ(
          owned_room_count(
              *owner),
          0U);

      EXPECT_FALSE(
          owns_room(
              *owner,
              first));

      EXPECT_FALSE(
          owns_room(
              *owner,
              second));
    }

    TEST(RoomOwnerTest, OwnershipIsIndependentBetweenNodes)
    {
      auto firstOwner =
          make_owner(
              make_node_id(
                  "node-1"));

      auto secondOwner =
          make_owner(
              make_node_id(
                  "node-2"));

      const RoomId firstRoom =
          make_room_id(
              "room/first");

      const RoomId secondRoom =
          make_room_id(
              "room/second");

      acquire_room(
          *firstOwner,
          firstRoom);

      acquire_room(
          *secondOwner,
          secondRoom);

      EXPECT_TRUE(
          owns_room(
              *firstOwner,
              firstRoom));

      EXPECT_FALSE(
          owns_room(
              *firstOwner,
              secondRoom));

      EXPECT_TRUE(
          owns_room(
              *secondOwner,
              secondRoom));

      EXPECT_FALSE(
          owns_room(
              *secondOwner,
              firstRoom));

      EXPECT_EQ(
          owner_node_id(
              *firstOwner),
          make_node_id(
              "node-1"));

      EXPECT_EQ(
          owner_node_id(
              *secondOwner),
          make_node_id(
              "node-2"));
    }

    TEST(RoomOwnerTest, RejectsEmptyRoomIdentifier)
    {
      auto owner =
          make_owner(
              make_node_id());

      EXPECT_FALSE(
          try_acquire_room(
              *owner,
              RoomId{}));

      EXPECT_EQ(
          owned_room_count(
              *owner),
          0U);

      EXPECT_TRUE(
          owner_empty(
              *owner));
    }

  } // namespace

} // namespace vix::realtime
