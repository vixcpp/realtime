/**
 *
 * @file session_constructor_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime logical session construction.
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
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <vix/json/json.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/session.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    [[nodiscard]] SessionId make_session_id()
    {
      return SessionId{
          std::string_view{
              "session-42"}};
    }

    [[nodiscard]] Timestamp make_created_at()
    {
      return Timestamp{
          std::chrono::seconds{
              1234}};
    }

    [[nodiscard]] JsonObject make_metadata()
    {
      JsonObject metadata;

      metadata.set_string(
          "transport",
          "websocket");

      metadata.set_i64(
          "attempt",
          2);

      return metadata;
    }

    TEST(SessionConstructorTest, ConstructsDetachedSession)
    {
      const Session session{
          make_session_id()};

      EXPECT_EQ(
          session.id(),
          make_session_id());

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

      EXPECT_EQ(
          session.room_count(),
          0U);
    }

    TEST(SessionConstructorTest, StoresIdentity)
    {
      const Session session{
          make_session_id(),
          Identity{
              "citizen-42"}};

      EXPECT_EQ(
          session.identity(),
          "citizen-42");
    }

    TEST(SessionConstructorTest, SupportsEmptyIdentity)
    {
      const Session session{
          make_session_id()};

      EXPECT_TRUE(
          session.identity().empty());
    }

    TEST(SessionConstructorTest, StoresResumeToken)
    {
      const Session session{
          make_session_id(),
          Identity{
              "citizen-42"},
          ResumeToken{
              "resume-token-42"}};

      EXPECT_EQ(
          session.resume_token(),
          "resume-token-42");
    }

    TEST(SessionConstructorTest, SupportsEmptyResumeToken)
    {
      const Session session{
          make_session_id()};

      EXPECT_TRUE(
          session.resume_token().empty());
    }

    TEST(SessionConstructorTest, StoresCreationTimestamp)
    {
      const Timestamp createdAt =
          make_created_at();

      const Session session{
          make_session_id(),
          Identity{
              "citizen-42"},
          ResumeToken{
              "resume-token-42"},
          createdAt};

      EXPECT_EQ(
          session.created_at(),
          createdAt);

      EXPECT_EQ(
          session.last_seen_at(),
          createdAt);
    }

    TEST(SessionConstructorTest, HasNoLifecycleTimestampsInitially)
    {
      const Session session{
          make_session_id(),
          Identity{},
          ResumeToken{},
          make_created_at()};

      EXPECT_FALSE(
          session.detached_at()
              .has_value());

      EXPECT_FALSE(
          session.closed_at()
              .has_value());
    }

    TEST(SessionConstructorTest, StoresMetadata)
    {
      const Session session{
          make_session_id(),
          Identity{
              "citizen-42"},
          ResumeToken{
              "resume-token-42"},
          make_created_at(),
          make_metadata()};

      const auto metadata =
          vix::json::to_json(
              session.metadata());

      EXPECT_EQ(
          metadata.at("transport")
              .get<std::string>(),
          "websocket");

      EXPECT_EQ(
          metadata.at("attempt")
              .get<std::int64_t>(),
          std::int64_t{2});
    }

    TEST(SessionConstructorTest, SupportsEmptyMetadata)
    {
      const Session session{
          make_session_id()};

      EXPECT_TRUE(
          session.metadata().empty());
    }

    TEST(SessionConstructorTest, RejectsEmptySessionIdentifier)
    {
      EXPECT_THROW(
          static_cast<void>(
              Session{
                  SessionId{}}),
          Error);
    }

    TEST(SessionConstructorTest, ConvertsStatesToString)
    {
      EXPECT_EQ(
          to_string(
              SessionState::Connected),
          std::string_view{
              "connected"});

      EXPECT_EQ(
          to_string(
              SessionState::Detached),
          std::string_view{
              "detached"});

      EXPECT_EQ(
          to_string(
              SessionState::Closed),
          std::string_view{
              "closed"});
    }

  } // namespace

} // namespace vix::realtime
