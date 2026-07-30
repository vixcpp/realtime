/**
 *
 * @file room_version_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime room version.
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
#include <string>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    TEST(RoomVersionTest, DefaultVersionIsInitial)
    {
      const RoomVersion version;

      EXPECT_TRUE(
          version.is_initial());

      EXPECT_EQ(
          version.value(),
          VersionValue{0});
    }

    TEST(RoomVersionTest, AcceptsZero)
    {
      const RoomVersion version{
          VersionValue{0}};

      EXPECT_TRUE(
          version.is_initial());

      EXPECT_EQ(
          version.value(),
          VersionValue{0});
    }

    TEST(RoomVersionTest, AcceptsPositiveValue)
    {
      const RoomVersion version{
          VersionValue{42}};

      EXPECT_FALSE(
          version.is_initial());

      EXPECT_EQ(
          version.value(),
          VersionValue{42});
    }

    TEST(RoomVersionTest, RejectsNegativeValue)
    {
      EXPECT_THROW(
          static_cast<void>(
              RoomVersion{
                  VersionValue{-1}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              RoomVersion{
                  std::numeric_limits<
                      VersionValue>::min()}),
          Error);
    }

    TEST(RoomVersionTest, NextReturnsFollowingVersion)
    {
      const RoomVersion version{
          VersionValue{41}};

      const RoomVersion next =
          version.next();

      EXPECT_EQ(
          version.value(),
          VersionValue{41});

      EXPECT_EQ(
          next.value(),
          VersionValue{42});
    }

    TEST(RoomVersionTest, NextFromInitialReturnsOne)
    {
      const RoomVersion version;

      const RoomVersion next =
          version.next();

      EXPECT_EQ(
          next.value(),
          VersionValue{1});

      EXPECT_FALSE(
          next.is_initial());
    }

    TEST(RoomVersionTest, IncrementMutatesVersion)
    {
      RoomVersion version{
          VersionValue{9}};

      version.increment();

      EXPECT_EQ(
          version.value(),
          VersionValue{10});
    }

    TEST(RoomVersionTest, IncrementCanBeAppliedRepeatedly)
    {
      RoomVersion version;

      version.increment();
      version.increment();
      version.increment();

      EXPECT_EQ(
          version.value(),
          VersionValue{3});
    }

    TEST(RoomVersionTest, NextRejectsOverflow)
    {
      const RoomVersion version{
          std::numeric_limits<
              VersionValue>::max()};

      EXPECT_THROW(
          static_cast<void>(
              version.next()),
          Error);
    }

    TEST(RoomVersionTest, IncrementRejectsOverflowWithoutWrapping)
    {
      RoomVersion version{
          std::numeric_limits<
              VersionValue>::max()};

      EXPECT_THROW(
          version.increment(),
          Error);

      EXPECT_EQ(
          version.value(),
          std::numeric_limits<
              VersionValue>::max());
    }

    TEST(RoomVersionTest, SupportsEquality)
    {
      const RoomVersion first{
          VersionValue{12}};

      const RoomVersion second{
          VersionValue{12}};

      const RoomVersion different{
          VersionValue{13}};

      EXPECT_EQ(first, second);
      EXPECT_NE(first, different);
    }

    TEST(RoomVersionTest, SupportsOrdering)
    {
      const RoomVersion older{
          VersionValue{12}};

      const RoomVersion newer{
          VersionValue{13}};

      EXPECT_LT(older, newer);
      EXPECT_GT(newer, older);
      EXPECT_LE(older, newer);
      EXPECT_GE(newer, older);
    }

    TEST(RoomVersionTest, ConvertsValueToString)
    {
      const RoomVersion initial;
      const RoomVersion version{
          VersionValue{42}};

      EXPECT_EQ(
          initial.value(),
          "0");

      EXPECT_EQ(
          version.value(),
          "42");
    }

  } // namespace

} // namespace vix::realtime
