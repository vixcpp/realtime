/**
 *
 * @file event_id_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime event identifier.
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

#include <limits>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    TEST(EventIdTest, DefaultIdentifierIsEmpty)
    {
      const EventId eventId;

      EXPECT_TRUE(eventId.empty());

      EXPECT_EQ(
          eventId.value(),
          EventIdValue{0});
    }

    TEST(EventIdTest, ZeroRepresentsNoPersistedEvent)
    {
      const EventId eventId{
          EventIdValue{0}};

      EXPECT_TRUE(eventId.empty());

      EXPECT_EQ(
          eventId.value(),
          EventIdValue{0});
    }

    TEST(EventIdTest, AcceptsPositiveIdentifier)
    {
      const EventId eventId{
          EventIdValue{42}};

      EXPECT_FALSE(eventId.empty());

      EXPECT_EQ(
          eventId.value(),
          EventIdValue{42});
    }

    TEST(EventIdTest, RejectsNegativeIdentifier)
    {
      EXPECT_THROW(
          static_cast<void>(
              EventId{
                  EventIdValue{-1}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              EventId{
                  std::numeric_limits<
                      EventIdValue>::min()}),
          Error);
    }

    TEST(EventIdTest, NextReturnsFollowingIdentifier)
    {
      const EventId eventId{
          EventIdValue{41}};

      const EventId next =
          eventId.next();

      EXPECT_EQ(
          eventId.value(),
          EventIdValue{41});

      EXPECT_EQ(
          next.value(),
          EventIdValue{42});
    }

    TEST(EventIdTest, NextFromEmptyReturnsOne)
    {
      const EventId eventId;

      const EventId next =
          eventId.next();

      EXPECT_FALSE(next.empty());

      EXPECT_EQ(
          next.value(),
          EventIdValue{1});
    }

    TEST(EventIdTest, IncrementMutatesIdentifier)
    {
      EventId eventId{
          EventIdValue{9}};

      eventId.increment();

      EXPECT_EQ(
          eventId.value(),
          EventIdValue{10});
    }

    TEST(EventIdTest, IncrementCanBeAppliedRepeatedly)
    {
      EventId eventId;

      eventId.increment();
      eventId.increment();
      eventId.increment();

      EXPECT_EQ(
          eventId.value(),
          EventIdValue{3});
    }

    TEST(EventIdTest, NextRejectsOverflow)
    {
      const EventId eventId{
          std::numeric_limits<
              EventIdValue>::max()};

      EXPECT_THROW(
          static_cast<void>(
              eventId.next()),
          Error);
    }

    TEST(EventIdTest, IncrementRejectsOverflowWithoutWrapping)
    {
      EventId eventId{
          std::numeric_limits<
              EventIdValue>::max()};

      EXPECT_THROW(
          eventId.increment(),
          Error);

      EXPECT_EQ(
          eventId.value(),
          std::numeric_limits<
              EventIdValue>::max());
    }

    TEST(EventIdTest, SupportsEquality)
    {
      const EventId first{
          EventIdValue{12}};

      const EventId second{
          EventIdValue{12}};

      const EventId different{
          EventIdValue{13}};

      EXPECT_EQ(first, second);
      EXPECT_NE(first, different);
    }

    TEST(EventIdTest, SupportsOrdering)
    {
      const EventId older{
          EventIdValue{12}};

      const EventId newer{
          EventIdValue{13}};

      EXPECT_LT(older, newer);
      EXPECT_GT(newer, older);
      EXPECT_LE(older, newer);
      EXPECT_GE(newer, older);
    }

    TEST(EventIdTest, ConvertsIdentifierToString)
    {
      const EventId empty;
      const EventId eventId{
          EventIdValue{42}};

      EXPECT_EQ(
          empty.value(),
          "0");

      EXPECT_EQ(
          eventId.value(),
          "42");
    }

  } // namespace

} // namespace vix::realtime
