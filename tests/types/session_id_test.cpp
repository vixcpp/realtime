/**
 *
 * @file session_id_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime logical session identifier.
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

#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/session_id.hpp>

namespace vix::realtime
{
  namespace
  {
    TEST(SessionIdTest, DefaultIdentifierIsEmpty)
    {
      const SessionId sessionId;

      EXPECT_TRUE(sessionId.empty());
      EXPECT_TRUE(sessionId.value().empty());
      EXPECT_TRUE(sessionId.view().empty());
      EXPECT_EQ(sessionId.size(), 0U);
    }

    TEST(SessionIdTest, AcceptsSimpleIdentifier)
    {
      const SessionId sessionId{
          std::string_view{"session42"}};

      EXPECT_FALSE(sessionId.empty());

      EXPECT_EQ(
          sessionId.value(),
          "session42");

      EXPECT_EQ(
          sessionId.view(),
          "session42");

      EXPECT_EQ(
          sessionId.size(),
          9U);
    }

    TEST(SessionIdTest, AcceptsSupportedSeparators)
    {
      const SessionId sessionId{
          std::string_view{
              "session_user-42.main"}};

      EXPECT_EQ(
          sessionId.value(),
          "session_user-42.main");
    }

    TEST(SessionIdTest, AcceptsMaximumLength)
    {
      const std::string value(
          SessionId::max_size,
          'a');

      const SessionId sessionId{
          std::string_view{value}};

      EXPECT_EQ(
          sessionId.size(),
          SessionId::max_size);

      EXPECT_EQ(
          sessionId.value(),
          value);
    }

    TEST(SessionIdTest, RejectsExplicitEmptyIdentifier)
    {
      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{}}),
          Error);
    }

    TEST(SessionIdTest, RejectsIdentifierAboveMaximumLength)
    {
      const std::string value(
          SessionId::max_size + 1,
          'a');

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{value}}),
          Error);
    }

    TEST(SessionIdTest, RejectsWhitespace)
    {
      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session user"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session\tuser"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session\nuser"}}),
          Error);
    }

    TEST(SessionIdTest, RejectsUnsupportedCharacters)
    {
      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session/user"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session:user"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session@user"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session#user"}}),
          Error);
    }

    TEST(SessionIdTest, RejectsLeadingSeparator)
    {
      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "-session"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "_session"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      ".session"}}),
          Error);
    }

    TEST(SessionIdTest, RejectsTrailingSeparator)
    {
      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session-"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session_"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session."}}),
          Error);
    }

    TEST(SessionIdTest, RejectsConsecutiveSeparators)
    {
      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session--user"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session__user"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session..user"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              SessionId{
                  std::string_view{
                      "session-_user"}}),
          Error);
    }

    TEST(SessionIdTest, StaticValidationAcceptsValidIdentifier)
    {
      EXPECT_TRUE(
          SessionId::is_valid(
              "session-42"));

      EXPECT_TRUE(
          SessionId::is_valid(
              "user_session.main"));
    }

    TEST(SessionIdTest, StaticValidationRejectsInvalidIdentifier)
    {
      EXPECT_FALSE(
          SessionId::is_valid(""));

      EXPECT_FALSE(
          SessionId::is_valid(
              "-session"));

      EXPECT_FALSE(
          SessionId::is_valid(
              "session--42"));

      EXPECT_FALSE(
          SessionId::is_valid(
              "session/42"));
    }

    TEST(SessionIdTest, SupportsEquality)
    {
      const SessionId first{
          std::string_view{
              "session-42"}};

      const SessionId second{
          std::string_view{
              "session-42"}};

      const SessionId different{
          std::string_view{
              "session-84"}};

      EXPECT_EQ(first, second);
      EXPECT_NE(first, different);
    }

    TEST(SessionIdTest, SupportsLexicographicalOrdering)
    {
      const SessionId first{
          std::string_view{
              "session-42"}};

      const SessionId second{
          std::string_view{
              "session-84"}};

      EXPECT_LT(first, second);
      EXPECT_GT(second, first);
      EXPECT_LE(first, second);
      EXPECT_GE(second, first);
    }

    TEST(SessionIdTest, HashIsStableForEqualIdentifiers)
    {
      const SessionId first{
          std::string_view{
              "session-42"}};

      const SessionId second{
          std::string_view{
              "session-42"}};

      const std::hash<SessionId> hash;

      EXPECT_EQ(
          hash(first),
          hash(second));
    }

    TEST(SessionIdTest, CanBeUsedInUnorderedContainers)
    {
      std::unordered_set<SessionId> sessionIds;

      sessionIds.emplace(
          std::string_view{
              "session-42"});

      sessionIds.emplace(
          std::string_view{
              "session-84"});

      sessionIds.emplace(
          std::string_view{
              "session-42"});

      EXPECT_EQ(
          sessionIds.size(),
          2U);

      EXPECT_EQ(
          sessionIds.count(
              SessionId{
                  std::string_view{
                      "session-42"}}),
          1U);

      EXPECT_EQ(
          sessionIds.count(
              SessionId{
                  std::string_view{
                      "session-unknown"}}),
          0U);
    }

    TEST(SessionIdTest, ConvertsIdentifierToString)
    {
      const SessionId sessionId{
          std::string_view{
              "session-42"}};

      EXPECT_EQ(
          to_string(sessionId),
          "session-42");
    }

  } // namespace

} // namespace vix::realtime
