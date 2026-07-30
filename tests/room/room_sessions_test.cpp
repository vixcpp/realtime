/**
 *
 * @file room_sessions_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for session membership in Vix Realtime rooms.
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
#include <string_view>
#include <utility>

#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/session.hpp>
#include <vix/realtime/session_id.hpp>
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
              room.join(*session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.join(
            *session);
      }
      else if constexpr (
          requires {
            room.join(*session);
          })
      {
        room.join(
            *session);

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
              room.add_session(session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.add_session(
            session);
      }
      else if constexpr (
          requires {
            room.add_session(session);
          })
      {
        room.add_session(
            session);

        return true;
      }
      else if constexpr (
          requires {
            {
              room.add_member(session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.add_member(
            session);
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room session join API");
      }
    }

    template <typename RoomType>
    [[nodiscard]] bool leave_session(
        RoomType &room,
        const std::shared_ptr<Session> &session)
    {
      if constexpr (
          requires {
            {
              room.leave(session->id())
            } -> std::convertible_to<bool>;
          })
      {
        return room.leave(
            session->id());
      }
      else if constexpr (
          requires {
            room.leave(
                session->id());
          })
      {
        room.leave(
            session->id());

        return true;
      }
      else if constexpr (
          requires {
            {
              room.leave(session)
            } -> std::convertible_to<bool>;
          })
      {
        return room.leave(
            session);
      }
      else if constexpr (
          requires {
            room.leave(session);
          })
      {
        room.leave(
            session);

        return true;
      }
      else if constexpr (
          requires {
            {
              room.leave_session(
                  session->id())
            } -> std::convertible_to<bool>;
          })
      {
        return room.leave_session(
            session->id());
      }
      else if constexpr (
          requires {
            {
              room.remove_session(
                  session->id())
            } -> std::convertible_to<bool>;
          })
      {
        return room.remove_session(
            session->id());
      }
      else if constexpr (
          requires {
            {
              room.remove_member(
                  session->id())
            } -> std::convertible_to<bool>;
          })
      {
        return room.remove_member(
            session->id());
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room session leave API");
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
      else if constexpr (
          requires {
            {
              room.contains_member(
                  sessionId)
            } -> std::convertible_to<bool>;
          })
      {
        return room.contains_member(
            sessionId);
      }
      else
      {
        static_assert(
            dependentFalse<RoomType>,
            "Unsupported Room membership lookup API");
      }
    }

    [[nodiscard]] RoomId make_room_id()
    {
      return RoomId{
          std::string_view{
              "city/river"}};
    }

    [[nodiscard]] SessionId make_session_id(
        std::string_view value)
    {
      return SessionId{
          value};
    }

    [[nodiscard]] std::shared_ptr<Session>
    make_session(
        std::string_view id)
    {
      return std::make_shared<Session>(
          make_session_id(
              id));
    }

    [[nodiscard]] std::unique_ptr<Room>
    make_room(
        Config config = {})
    {
      auto room =
          std::make_unique<Room>(
              make_room_id(),
              std::make_unique<EmptyState>(),
              std::make_unique<EmptyHandler>(),
              std::make_shared<
                  MemoryEventStore>(),
              std::make_shared<
                  MemorySnapshotStore>(),
              std::move(config));

      room->open();

      return room;
    }

    TEST(RoomSessionsTest, StartsWithoutSessions)
    {
      const auto room =
          make_room();

      EXPECT_EQ(
          room->member_count(),
          0U);

      EXPECT_TRUE(
          room->empty());

      EXPECT_FALSE(
          has_session(
              *room,
              make_session_id(
                  "session-1")));
    }

    TEST(RoomSessionsTest, JoinsSession)
    {
      auto room =
          make_room();

      const auto session =
          make_session(
              "session-1");

      EXPECT_TRUE(
          join_session(
              *room,
              session));

      EXPECT_EQ(
          room->member_count(),
          1U);

      EXPECT_FALSE(
          room->empty());

      EXPECT_TRUE(
          has_session(
              *room,
              session->id()));
    }

    TEST(RoomSessionsTest, JoinUpdatesSessionMembership)
    {
      auto room =
          make_room();

      const auto session =
          make_session(
              "session-1");

      ASSERT_TRUE(
          join_session(
              *room,
              session));

      EXPECT_TRUE(
          session->has_room(
              make_room_id()));

      EXPECT_EQ(
          session->room_count(),
          1U);
    }

    TEST(RoomSessionsTest, JoiningSameSessionTwiceIsIdempotent)
    {
      auto room =
          make_room();

      const auto session =
          make_session(
              "session-1");

      ASSERT_TRUE(
          join_session(
              *room,
              session));

      static_cast<void>(
          join_session(
              *room,
              session));

      EXPECT_EQ(
          room->member_count(),
          1U);

      EXPECT_EQ(
          session->room_count(),
          1U);
    }

    TEST(RoomSessionsTest, JoinsMultipleSessions)
    {
      auto room =
          make_room();

      const auto first =
          make_session(
              "session-1");

      const auto second =
          make_session(
              "session-2");

      const auto third =
          make_session(
              "session-3");

      EXPECT_TRUE(
          join_session(
              *room,
              first));

      EXPECT_TRUE(
          join_session(
              *room,
              second));

      EXPECT_TRUE(
          join_session(
              *room,
              third));

      EXPECT_EQ(
          room->member_count(),
          3U);

      EXPECT_TRUE(
          has_session(
              *room,
              first->id()));

      EXPECT_TRUE(
          has_session(
              *room,
              second->id()));

      EXPECT_TRUE(
          has_session(
              *room,
              third->id()));
    }

    TEST(RoomSessionsTest, LeavesSession)
    {
      auto room =
          make_room();

      const auto first =
          make_session(
              "session-1");

      const auto second =
          make_session(
              "session-2");

      join_session(
          *room,
          first);

      join_session(
          *room,
          second);

      EXPECT_TRUE(
          leave_session(
              *room,
              first));

      EXPECT_EQ(
          room->member_count(),
          1U);

      EXPECT_FALSE(
          has_session(
              *room,
              first->id()));

      EXPECT_TRUE(
          has_session(
              *room,
              second->id()));
    }

    TEST(RoomSessionsTest, LeaveUpdatesSessionMembership)
    {
      auto room =
          make_room();

      const auto session =
          make_session(
              "session-1");

      join_session(
          *room,
          session);

      ASSERT_TRUE(
          session->has_room(
              make_room_id()));

      leave_session(
          *room,
          session);

      EXPECT_FALSE(
          session->has_room(
              make_room_id()));

      EXPECT_EQ(
          session->room_count(),
          0U);
    }

    TEST(RoomSessionsTest, LeavingUnknownSessionDoesNotChangeRoom)
    {
      auto room =
          make_room();

      const auto joined =
          make_session(
              "session-1");

      const auto unknown =
          make_session(
              "session-2");

      join_session(
          *room,
          joined);

      static_cast<void>(
          leave_session(
              *room,
              unknown));

      EXPECT_EQ(
          room->member_count(),
          1U);

      EXPECT_TRUE(
          has_session(
              *room,
              joined->id()));
    }

    TEST(RoomSessionsTest, RoomBecomesEmptyAfterLastSessionLeaves)
    {
      auto room =
          make_room();

      const auto session =
          make_session(
              "session-1");

      join_session(
          *room,
          session);

      leave_session(
          *room,
          session);

      EXPECT_EQ(
          room->member_count(),
          0U);

      EXPECT_TRUE(
          room->empty());
    }

    TEST(RoomSessionsTest, RespectsMaximumSessionLimit)
    {
      Config config;

      config.maxSessionsPerRoom = 2;

      auto room =
          make_room(
              config);

      const auto first =
          make_session(
              "session-1");

      const auto second =
          make_session(
              "session-2");

      const auto third =
          make_session(
              "session-3");

      join_session(
          *room,
          first);

      join_session(
          *room,
          second);

      try
      {
        static_cast<void>(
            join_session(
                *room,
                third));
      }
      catch (const Error &)
      {
      }

      EXPECT_EQ(
          room->member_count(),
          2U);

      EXPECT_TRUE(
          has_session(
              *room,
              first->id()));

      EXPECT_TRUE(
          has_session(
              *room,
              second->id()));

      EXPECT_FALSE(
          has_session(
              *room,
              third->id()));

      EXPECT_FALSE(
          third->has_room(
              make_room_id()));
    }

    TEST(RoomSessionsTest, LeavingSessionFreesCapacity)
    {
      Config config;

      config.maxSessionsPerRoom = 1;

      auto room =
          make_room(
              config);

      const auto first =
          make_session(
              "session-1");

      const auto second =
          make_session(
              "session-2");

      join_session(
          *room,
          first);

      leave_session(
          *room,
          first);

      EXPECT_TRUE(
          join_session(
              *room,
              second));

      EXPECT_EQ(
          room->member_count(),
          1U);

      EXPECT_TRUE(
          has_session(
              *room,
              second->id()));
    }

  } // namespace

} // namespace vix::realtime
