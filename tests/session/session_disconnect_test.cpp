/**
 *
 * @file session_disconnect_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for disconnecting and closing Vix Realtime sessions.
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
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/session.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    class TestConnection final : public Connection
    {
    public:
      explicit TestConnection(
          std::string id)
          : id_(std::move(id))
      {
      }

      [[nodiscard]] const ConnectionId &
      id() const noexcept override
      {
        return id_;
      }

      [[nodiscard]] bool
      is_open() const noexcept override
      {
        return open_;
      }

      void send(
          const protocol::Envelope &) override
      {
        ++sendCount_;
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
      metadata() const noexcept override
      {
        return metadata_;
      }

      [[nodiscard]] std::size_t
      send_count() const noexcept
      {
        return sendCount_;
      }

      [[nodiscard]] std::size_t
      close_count() const noexcept
      {
        return closeCount_;
      }

      [[nodiscard]] ErrorCode
      close_code() const noexcept
      {
        return closeCode_;
      }

      [[nodiscard]] const std::string &
      close_reason() const noexcept
      {
        return closeReason_;
      }

    private:
      ConnectionId id_{};
      bool open_{true};
      std::size_t sendCount_{0};
      std::size_t closeCount_{0};
      ErrorCode closeCode_{ErrorCode::None};
      std::string closeReason_{};
      JsonObject metadata_{};
    };

    [[nodiscard]] Session make_session()
    {
      return Session{
          SessionId{
              std::string_view{
                  "session-42"}},
          Identity{
              "citizen-42"},
          ResumeToken{
              "resume-token-42"},
          Timestamp{
              std::chrono::seconds{
                  1000}}};
    }

    [[nodiscard]] ConnectionPtr make_connection(
        std::string id = "connection-42")
    {
      return std::make_shared<TestConnection>(
          std::move(id));
    }

    TEST(SessionDisconnectTest, AttachesConnection)
    {
      Session session =
          make_session();

      const ConnectionPtr connection =
          make_connection();

      const Timestamp attachedAt{
          std::chrono::seconds{
              1100}};

      session.attach(
          connection,
          attachedAt);

      EXPECT_EQ(
          session.state(),
          SessionState::Connected);

      EXPECT_TRUE(
          session.connected());

      EXPECT_FALSE(
          session.detached());

      EXPECT_FALSE(
          session.closed());

      EXPECT_EQ(
          session.connection(),
          connection);

      EXPECT_EQ(
          session.last_seen_at(),
          attachedAt);

      EXPECT_FALSE(
          session.detached_at()
              .has_value());
    }

    TEST(SessionDisconnectTest, RejectsNullConnection)
    {
      Session session =
          make_session();

      EXPECT_THROW(
          session.attach(
              nullptr),
          Error);

      EXPECT_TRUE(
          session.detached());

      EXPECT_EQ(
          session.connection(),
          nullptr);
    }

    TEST(SessionDisconnectTest, ReplacesExistingConnection)
    {
      Session session =
          make_session();

      const ConnectionPtr first =
          make_connection(
              "connection-1");

      const ConnectionPtr second =
          make_connection(
              "connection-2");

      session.attach(
          first);

      session.attach(
          second);

      EXPECT_EQ(
          session.connection(),
          second);

      EXPECT_TRUE(
          session.connected());
    }

    TEST(SessionDisconnectTest, DetachesConnection)
    {
      Session session =
          make_session();

      const ConnectionPtr connection =
          make_connection();

      session.attach(
          connection);

      const Timestamp detachedAt{
          std::chrono::seconds{
              1200}};

      session.detach(
          detachedAt);

      EXPECT_EQ(
          session.state(),
          SessionState::Detached);

      EXPECT_FALSE(
          session.connected());

      EXPECT_TRUE(
          session.detached());

      EXPECT_FALSE(
          session.closed());

      EXPECT_EQ(
          session.connection(),
          nullptr);

      ASSERT_TRUE(
          session.detached_at()
              .has_value());

      EXPECT_EQ(
          *session.detached_at(),
          detachedAt);

      EXPECT_EQ(
          session.last_seen_at(),
          detachedAt);
    }

    TEST(SessionDisconnectTest, DetachingDoesNotCloseTransport)
    {
      Session session =
          make_session();

      const auto connection =
          std::make_shared<TestConnection>(
              "connection-42");

      session.attach(
          connection);

      session.detach();

      EXPECT_EQ(
          connection->close_count(),
          0U);

      EXPECT_TRUE(
          connection->is_open());
    }

    TEST(SessionDisconnectTest, CanAttachAfterDetach)
    {
      Session session =
          make_session();

      const ConnectionPtr first =
          make_connection(
              "connection-1");

      const ConnectionPtr second =
          make_connection(
              "connection-2");

      session.attach(
          first);

      session.detach(
          Timestamp{
              std::chrono::seconds{
                  1200}});

      session.attach(
          second,
          Timestamp{
              std::chrono::seconds{
                  1300}});

      EXPECT_TRUE(
          session.connected());

      EXPECT_EQ(
          session.connection(),
          second);

      EXPECT_FALSE(
          session.detached_at()
              .has_value());

      EXPECT_EQ(
          session.last_seen_at(),
          Timestamp{
              std::chrono::seconds{
                  1300}});
    }

    TEST(SessionDisconnectTest, TouchUpdatesLastSeenTimestamp)
    {
      Session session =
          make_session();

      const Timestamp lastSeenAt{
          std::chrono::seconds{
              1400}};

      session.touch(
          lastSeenAt);

      EXPECT_EQ(
          session.last_seen_at(),
          lastSeenAt);
    }

    TEST(SessionDisconnectTest, CloseTransitionsSessionToClosed)
    {
      Session session =
          make_session();

      const Timestamp closedAt{
          std::chrono::seconds{
              1500}};

      session.close(
          closedAt);

      EXPECT_EQ(
          session.state(),
          SessionState::Closed);

      EXPECT_FALSE(
          session.connected());

      EXPECT_FALSE(
          session.detached());

      EXPECT_TRUE(
          session.closed());

      EXPECT_EQ(
          session.connection(),
          nullptr);

      ASSERT_TRUE(
          session.closed_at()
              .has_value());

      EXPECT_EQ(
          *session.closed_at(),
          closedAt);

      EXPECT_EQ(
          session.last_seen_at(),
          closedAt);
    }

    TEST(SessionDisconnectTest, ClosingConnectedSessionClosesConnection)
    {
      Session session =
          make_session();

      const auto connection =
          std::make_shared<TestConnection>(
              "connection-42");

      session.attach(
          connection);

      session.close(
          Timestamp{
              std::chrono::seconds{
                  1500}});

      EXPECT_EQ(
          connection->close_count(),
          1U);

      EXPECT_FALSE(
          connection->is_open());

      EXPECT_EQ(
          session.connection(),
          nullptr);

      EXPECT_TRUE(
          session.closed());
    }

    TEST(SessionDisconnectTest, CloseRemovesRoomMemberships)
    {
      Session session =
          make_session();

      session.join_room(
          RoomId{
              std::string_view{
                  "chat/general"}});

      session.join_room(
          RoomId{
              std::string_view{
                  "chat/private"}});

      ASSERT_EQ(
          session.room_count(),
          2U);

      session.close();

      EXPECT_EQ(
          session.room_count(),
          0U);

      EXPECT_TRUE(
          session.rooms().empty());
    }

    TEST(SessionDisconnectTest, CannotAttachClosedSession)
    {
      Session session =
          make_session();

      session.close();

      EXPECT_THROW(
          session.attach(
              make_connection()),
          Error);

      EXPECT_TRUE(
          session.closed());

      EXPECT_EQ(
          session.connection(),
          nullptr);
    }

    TEST(SessionDisconnectTest, ClosingTwiceIsIdempotent)
    {
      Session session =
          make_session();

      const Timestamp firstClosedAt{
          std::chrono::seconds{
              1500}};

      session.close(
          firstClosedAt);

      session.close(
          Timestamp{
              std::chrono::seconds{
                  1600}});

      ASSERT_TRUE(
          session.closed_at()
              .has_value());

      EXPECT_EQ(
          *session.closed_at(),
          firstClosedAt);

      EXPECT_TRUE(
          session.closed());
    }

  } // namespace

} // namespace vix::realtime
