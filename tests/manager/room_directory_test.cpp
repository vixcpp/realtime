/**
 *
 * @file room_directory_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime local room directory.
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

#include <atomic>
#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_directory.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
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

    template <typename DirectoryType>
    [[nodiscard]] bool register_room(
        DirectoryType &directory,
        const std::shared_ptr<Room> &room)
    {
      if constexpr (
          requires {
            {
              directory.register_room(room)
            } -> std::convertible_to<bool>;
          })
      {
        return directory.register_room(
            room);
      }
      else if constexpr (
          requires {
            directory.register_room(room);
          })
      {
        directory.register_room(
            room);

        return true;
      }
      else if constexpr (
          requires {
            {
              directory.register_room(
                  room->id(),
                  room)
            } -> std::convertible_to<bool>;
          })
      {
        return directory.register_room(
            room->id(),
            room);
      }
      else if constexpr (
          requires {
            directory.register_room(
                room->id(),
                room);
          })
      {
        directory.register_room(
            room->id(),
            room);

        return true;
      }
      else if constexpr (
          requires {
            {
              directory.add(room)
            } -> std::convertible_to<bool>;
          })
      {
        return directory.add(
            room);
      }
      else if constexpr (
          requires {
            directory.add(room);
          })
      {
        directory.add(
            room);

        return true;
      }
      else if constexpr (
          requires {
            {
              directory.insert(room)
            } -> std::convertible_to<bool>;
          })
      {
        return directory.insert(
            room);
      }
      else if constexpr (
          requires {
            {
              directory.emplace(
                  room->id(),
                  room)
            } -> std::convertible_to<bool>;
          })
      {
        return directory.emplace(
            room->id(),
            room);
      }
      else
      {
        static_assert(
            dependentFalse<DirectoryType>,
            "Unsupported RoomDirectory registration API");
      }
    }

    template <typename ValueType>
    [[nodiscard]] Room *extract_room_pointer(
        const ValueType &value)
    {
      using Value =
          std::remove_cvref_t<
              ValueType>;

      if constexpr (
          std::same_as<
              Value,
              std::shared_ptr<Room>>)
      {
        return value.get();
      }
      else if constexpr (
          std::same_as<
              Value,
              std::weak_ptr<Room>>)
      {
        return value.lock()
            .get();
      }
      else if constexpr (
          std::is_pointer_v<Value>)
      {
        return value;
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

        return extract_room_pointer(
            *value);
      }
      else if constexpr (
          requires {
            value.lock();
          })
      {
        return value.lock()
            .get();
      }
      else if constexpr (
          requires {
            value.get();
          })
      {
        return value.get();
      }
      else
      {
        static_assert(
            dependentFalse<Value>,
            "Unsupported RoomDirectory lookup result");
      }
    }

    template <typename DirectoryType>
    [[nodiscard]] Room *find_room(
        const DirectoryType &directory,
        const RoomId &roomId)
    {
      if constexpr (
          requires {
            directory.find(
                roomId);
          })
      {
        const auto result =
            directory.find(
                roomId);

        return extract_room_pointer(
            result);
      }
      else if constexpr (
          requires {
            directory.find_room(
                roomId);
          })
      {
        const auto result =
            directory.find_room(
                roomId);

        return extract_room_pointer(
            result);
      }
      else if constexpr (
          requires {
            directory.get(
                roomId);
          })
      {
        const auto result =
            directory.get(
                roomId);

        return extract_room_pointer(
            result);
      }
      else if constexpr (
          requires {
            directory.room(
                roomId);
          })
      {
        const auto result =
            directory.room(
                roomId);

        return extract_room_pointer(
            result);
      }
      else
      {
        static_assert(
            dependentFalse<DirectoryType>,
            "Unsupported RoomDirectory lookup API");
      }
    }

    template <typename DirectoryType>
    [[nodiscard]] bool contains_room(
        const DirectoryType &directory,
        const RoomId &roomId)
    {
      if constexpr (
          requires {
            {
              directory.contains(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return directory.contains(
            roomId);
      }
      else if constexpr (
          requires {
            {
              directory.contains_room(
                  roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return directory.contains_room(
            roomId);
      }
      else if constexpr (
          requires {
            {
              directory.has_room(
                  roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return directory.has_room(
            roomId);
      }
      else
      {
        return find_room(
                   directory,
                   roomId) != nullptr;
      }
    }

    template <typename DirectoryType>
    [[nodiscard]] bool remove_room(
        DirectoryType &directory,
        const RoomId &roomId)
    {
      if constexpr (
          requires {
            {
              directory.remove(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return directory.remove(
            roomId);
      }
      else if constexpr (
          requires {
            directory.remove(
                roomId);
          })
      {
        directory.remove(
            roomId);

        return true;
      }
      else if constexpr (
          requires {
            {
              directory.unregister_room(
                  roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return directory.unregister_room(
            roomId);
      }
      else if constexpr (
          requires {
            directory.unregister_room(
                roomId);
          })
      {
        directory.unregister_room(
            roomId);

        return true;
      }
      else if constexpr (
          requires {
            {
              directory.erase(roomId)
            } -> std::convertible_to<bool>;
          })
      {
        return directory.erase(
            roomId);
      }
      else
      {
        static_assert(
            dependentFalse<DirectoryType>,
            "Unsupported RoomDirectory removal API");
      }
    }

    template <typename DirectoryType>
    [[nodiscard]] std::size_t directory_size(
        const DirectoryType &directory)
    {
      if constexpr (
          requires {
            {
              directory.size()
            } -> std::convertible_to<std::size_t>;
          })
      {
        return static_cast<std::size_t>(
            directory.size());
      }
      else if constexpr (
          requires {
            {
              directory.room_count()
            } -> std::convertible_to<std::size_t>;
          })
      {
        return static_cast<std::size_t>(
            directory.room_count());
      }
      else if constexpr (
          requires {
            {
              directory.count()
            } -> std::convertible_to<std::size_t>;
          })
      {
        return static_cast<std::size_t>(
            directory.count());
      }
      else
      {
        static_assert(
            dependentFalse<DirectoryType>,
            "Unsupported RoomDirectory size API");
      }
    }

    template <typename DirectoryType>
    [[nodiscard]] bool directory_empty(
        const DirectoryType &directory)
    {
      if constexpr (
          requires {
            {
              directory.empty()
            } -> std::convertible_to<bool>;
          })
      {
        return directory.empty();
      }
      else
      {
        return directory_size(
                   directory) == 0U;
      }
    }

    template <typename DirectoryType>
    void clear_directory(
        DirectoryType &directory)
    {
      if constexpr (
          requires {
            directory.clear();
          })
      {
        directory.clear();
      }
      else if constexpr (
          requires {
            directory.clear_rooms();
          })
      {
        directory.clear_rooms();
      }
      else if constexpr (
          requires {
            directory.reset();
          })
      {
        directory.reset();
      }
      else
      {
        static_assert(
            dependentFalse<DirectoryType>,
            "Unsupported RoomDirectory clear API");
      }
    }

    [[nodiscard]] RoomId make_room_id(
        std::string_view value)
    {
      return RoomId{
          value};
    }

    [[nodiscard]] std::shared_ptr<Room>
    make_room(
        RoomId roomId)
    {
      auto room =
          std::make_shared<Room>(
              std::move(roomId),
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

    [[nodiscard]] bool try_register_room(
        RoomDirectory &directory,
        const std::shared_ptr<Room> &room)
    {
      try
      {
        return register_room(
            directory,
            room);
      }
      catch (const std::exception &)
      {
        return false;
      }
    }

    TEST(RoomDirectoryTest, StartsEmpty)
    {
      const RoomDirectory directory;

      EXPECT_TRUE(
          directory_empty(
              directory));

      EXPECT_EQ(
          directory_size(
              directory),
          0U);

      EXPECT_FALSE(
          contains_room(
              directory,
              make_room_id(
                  "room/missing")));

      EXPECT_EQ(
          find_room(
              directory,
              make_room_id(
                  "room/missing")),
          nullptr);
    }

    TEST(RoomDirectoryTest, RegistersRoom)
    {
      RoomDirectory directory;

      const auto room =
          make_room(
              make_room_id(
                  "room/main"));

      EXPECT_TRUE(
          register_room(
              directory,
              room));

      EXPECT_EQ(
          directory_size(
              directory),
          1U);

      EXPECT_FALSE(
          directory_empty(
              directory));

      EXPECT_TRUE(
          contains_room(
              directory,
              room->id()));
    }

    TEST(RoomDirectoryTest, FindsRegisteredRoom)
    {
      RoomDirectory directory;

      const auto room =
          make_room(
              make_room_id(
                  "room/main"));

      register_room(
          directory,
          room);

      Room *found =
          find_room(
              directory,
              room->id());

      ASSERT_NE(
          found,
          nullptr);

      EXPECT_EQ(
          found,
          room.get());

      EXPECT_EQ(
          found->id(),
          room->id());
    }

    TEST(RoomDirectoryTest, KeepsRegisteredRoomAlive)
    {
      RoomDirectory directory;

      const RoomId roomId =
          make_room_id(
              "room/main");

      auto room =
          make_room(
              roomId);

      Room *original =
          room.get();

      register_room(
          directory,
          room);

      room.reset();

      Room *found =
          find_room(
              directory,
              roomId);

      ASSERT_NE(
          found,
          nullptr);

      EXPECT_EQ(
          found,
          original);

      EXPECT_TRUE(
          found->is_open());
    }

    TEST(RoomDirectoryTest, PreventsDuplicateRoomRegistration)
    {
      RoomDirectory directory;

      const RoomId roomId =
          make_room_id(
              "room/main");

      const auto first =
          make_room(
              roomId);

      const auto second =
          make_room(
              roomId);

      ASSERT_TRUE(
          try_register_room(
              directory,
              first));

      static_cast<void>(
          try_register_room(
              directory,
              second));

      EXPECT_EQ(
          directory_size(
              directory),
          1U);

      EXPECT_EQ(
          find_room(
              directory,
              roomId),
          first.get());
    }

    TEST(RoomDirectoryTest, RegistersMultipleRooms)
    {
      RoomDirectory directory;

      const auto first =
          make_room(
              make_room_id(
                  "room/first"));

      const auto second =
          make_room(
              make_room_id(
                  "room/second"));

      const auto third =
          make_room(
              make_room_id(
                  "room/third"));

      EXPECT_TRUE(
          register_room(
              directory,
              first));

      EXPECT_TRUE(
          register_room(
              directory,
              second));

      EXPECT_TRUE(
          register_room(
              directory,
              third));

      EXPECT_EQ(
          directory_size(
              directory),
          3U);

      EXPECT_EQ(
          find_room(
              directory,
              first->id()),
          first.get());

      EXPECT_EQ(
          find_room(
              directory,
              second->id()),
          second.get());

      EXPECT_EQ(
          find_room(
              directory,
              third->id()),
          third.get());
    }

    TEST(RoomDirectoryTest, RemovesRegisteredRoom)
    {
      RoomDirectory directory;

      const auto room =
          make_room(
              make_room_id(
                  "room/main"));

      register_room(
          directory,
          room);

      EXPECT_TRUE(
          remove_room(
              directory,
              room->id()));

      EXPECT_EQ(
          directory_size(
              directory),
          0U);

      EXPECT_FALSE(
          contains_room(
              directory,
              room->id()));

      EXPECT_EQ(
          find_room(
              directory,
              room->id()),
          nullptr);
    }

    TEST(RoomDirectoryTest, RemovingUnknownRoomDoesNotChangeDirectory)
    {
      RoomDirectory directory;

      const auto room =
          make_room(
              make_room_id(
                  "room/main"));

      register_room(
          directory,
          room);

      static_cast<void>(
          remove_room(
              directory,
              make_room_id(
                  "room/missing")));

      EXPECT_EQ(
          directory_size(
              directory),
          1U);

      EXPECT_EQ(
          find_room(
              directory,
              room->id()),
          room.get());
    }

    TEST(RoomDirectoryTest, RemovedRoomCanBeRegisteredAgain)
    {
      RoomDirectory directory;

      const RoomId roomId =
          make_room_id(
              "room/main");

      const auto first =
          make_room(
              roomId);

      register_room(
          directory,
          first);

      remove_room(
          directory,
          roomId);

      const auto replacement =
          make_room(
              roomId);

      EXPECT_TRUE(
          register_room(
              directory,
              replacement));

      EXPECT_EQ(
          directory_size(
              directory),
          1U);

      EXPECT_EQ(
          find_room(
              directory,
              roomId),
          replacement.get());
    }

    TEST(RoomDirectoryTest, ClearsAllRooms)
    {
      RoomDirectory directory;

      const auto first =
          make_room(
              make_room_id(
                  "room/first"));

      const auto second =
          make_room(
              make_room_id(
                  "room/second"));

      register_room(
          directory,
          first);

      register_room(
          directory,
          second);

      ASSERT_EQ(
          directory_size(
              directory),
          2U);

      clear_directory(
          directory);

      EXPECT_TRUE(
          directory_empty(
              directory));

      EXPECT_EQ(
          directory_size(
              directory),
          0U);

      EXPECT_EQ(
          find_room(
              directory,
              first->id()),
          nullptr);

      EXPECT_EQ(
          find_room(
              directory,
              second->id()),
          nullptr);
    }

    TEST(RoomDirectoryTest, SupportsConcurrentUniqueRegistrations)
    {
      RoomDirectory directory;

      constexpr std::size_t roomCount = 16;

      std::vector<std::shared_ptr<Room>>
          rooms;

      rooms.reserve(
          roomCount);

      for (std::size_t index = 0;
           index < roomCount;
           ++index)
      {
        rooms.push_back(
            make_room(
                make_room_id(
                    "room/" +
                    std::to_string(
                        index))));
      }

      std::atomic<std::size_t>
          registeredCount{0};

      std::vector<std::thread>
          threads;

      threads.reserve(
          roomCount);

      for (const auto &room : rooms)
      {
        threads.emplace_back(
            [&directory,
             &registeredCount,
             room]
            {
              if (try_register_room(
                      directory,
                      room))
              {
                registeredCount.fetch_add(
                    1,
                    std::memory_order_relaxed);
              }
            });
      }

      for (std::thread &thread : threads)
      {
        thread.join();
      }

      EXPECT_EQ(
          registeredCount.load(
              std::memory_order_relaxed),
          roomCount);

      EXPECT_EQ(
          directory_size(
              directory),
          roomCount);

      for (const auto &room : rooms)
      {
        EXPECT_EQ(
            find_room(
                directory,
                room->id()),
            room.get());
      }
    }

  } // namespace

} // namespace vix::realtime
