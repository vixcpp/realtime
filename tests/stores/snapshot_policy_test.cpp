/**
 *
 * @file snapshot_policy_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime snapshot creation policy.
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

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <vix/realtime/config.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/internal/snapshot_policy.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime::internal
{
  namespace
  {
    [[nodiscard]] RoomId make_room_id()
    {
      return RoomId{
          std::string_view{
              "counter/main"}};
    }

    [[nodiscard]] RoomSnapshot make_snapshot(
        VersionValue version,
        EventIdValue eventId)
    {
      JsonObject state;

      state.set_i64(
          "value",
          version);

      return RoomSnapshot{
          make_room_id(),
          RoomVersion{
              version},
          EventId{
              eventId},
          std::move(state),
          SchemaVersion{1}};
    }

    TEST(SnapshotReasonTest, ConvertsReasonsToString)
    {
      EXPECT_EQ(
          to_string(
              SnapshotReason::None),
          std::string_view{
              "none"});

      EXPECT_EQ(
          to_string(
              SnapshotReason::EventInterval),
          std::string_view{
              "event_interval"});

      EXPECT_EQ(
          to_string(
              SnapshotReason::RoomClose),
          std::string_view{
              "room_close"});

      EXPECT_EQ(
          to_string(
              SnapshotReason::Explicit),
          std::string_view{
              "explicit"});
    }

    TEST(SnapshotPolicyTest, StoresConfiguration)
    {
      const SnapshotPolicy policy{
          100,
          3,
          true};

      EXPECT_EQ(
          policy.every_events(),
          100U);

      EXPECT_EQ(
          policy.snapshots_to_keep(),
          3U);

      EXPECT_TRUE(
          policy.snapshot_on_room_close());

      EXPECT_TRUE(
          policy.interval_enabled());
    }

    TEST(SnapshotPolicyTest, SupportsDisabledEventInterval)
    {
      const SnapshotPolicy policy{
          0,
          3,
          true};

      EXPECT_EQ(
          policy.every_events(),
          0U);

      EXPECT_FALSE(
          policy.interval_enabled());
    }

    TEST(SnapshotPolicyTest, RejectsZeroRetention)
    {
      EXPECT_THROW(
          static_cast<void>(
              SnapshotPolicy{
                  100,
                  0,
                  true}),
          Error);
    }

    TEST(SnapshotPolicyTest, CreatesPolicyFromConfig)
    {
      Config config;

      config.snapshotEveryEvents = 50;
      config.snapshotsToKeep = 7;
      config.snapshotOnRoomClose = false;

      const SnapshotPolicy policy =
          SnapshotPolicy::from_config(
              config);

      EXPECT_EQ(
          policy.every_events(),
          50U);

      EXPECT_EQ(
          policy.snapshots_to_keep(),
          7U);

      EXPECT_FALSE(
          policy.snapshot_on_room_close());
    }

    TEST(SnapshotPolicyTest, DoesNotSnapshotInitialEmptyRoom)
    {
      const SnapshotPolicy policy{
          10,
          3,
          true};

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{},
              EventId{});

      EXPECT_FALSE(
          decision.required);

      EXPECT_FALSE(
          static_cast<bool>(
              decision));

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::None);

      EXPECT_EQ(
          decision.eventsSinceSnapshot,
          0U);

      EXPECT_EQ(
          decision.snapshotsToKeep,
          3U);
    }

    TEST(SnapshotPolicyTest, DoesNotSnapshotBeforeInterval)
    {
      const SnapshotPolicy policy{
          10,
          3,
          false};

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{9}},
              EventId{
                  EventIdValue{9}});

      EXPECT_FALSE(
          decision.required);

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::None);

      EXPECT_EQ(
          decision.eventsSinceSnapshot,
          9U);
    }

    TEST(SnapshotPolicyTest, SnapshotsAtEventInterval)
    {
      const SnapshotPolicy policy{
          10,
          3,
          false};

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{10}},
              EventId{
                  EventIdValue{10}});

      EXPECT_TRUE(
          decision.required);

      EXPECT_TRUE(
          static_cast<bool>(
              decision));

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::EventInterval);

      EXPECT_EQ(
          decision.eventsSinceSnapshot,
          10U);

      EXPECT_EQ(
          decision.snapshotsToKeep,
          3U);
    }

    TEST(SnapshotPolicyTest, SnapshotsWhenIntervalIsExceeded)
    {
      const SnapshotPolicy policy{
          10,
          3,
          false};

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{14}},
              EventId{
                  EventIdValue{14}});

      EXPECT_TRUE(
          decision.required);

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::EventInterval);

      EXPECT_EQ(
          decision.eventsSinceSnapshot,
          14U);
    }

    TEST(SnapshotPolicyTest, CountsEventsSinceLatestSnapshot)
    {
      const SnapshotPolicy policy{
          10,
          3,
          false};

      const RoomSnapshot latest =
          make_snapshot(
              VersionValue{20},
              EventIdValue{20});

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{27}},
              EventId{
                  EventIdValue{27}},
              &latest);

      EXPECT_FALSE(
          decision.required);

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::None);

      EXPECT_EQ(
          decision.eventsSinceSnapshot,
          7U);
    }

    TEST(SnapshotPolicyTest, AppliesIntervalAfterLatestSnapshot)
    {
      const SnapshotPolicy policy{
          10,
          3,
          false};

      const RoomSnapshot latest =
          make_snapshot(
              VersionValue{20},
              EventIdValue{20});

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{30}},
              EventId{
                  EventIdValue{30}},
              &latest);

      EXPECT_TRUE(
          decision.required);

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::EventInterval);

      EXPECT_EQ(
          decision.eventsSinceSnapshot,
          10U);
    }

    TEST(SnapshotPolicyTest, ExplicitRequestCreatesSnapshot)
    {
      const SnapshotPolicy policy{
          100,
          3,
          false};

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{4}},
              EventId{
                  EventIdValue{4}},
              nullptr,
              false,
              true);

      EXPECT_TRUE(
          decision.required);

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::Explicit);

      EXPECT_EQ(
          decision.eventsSinceSnapshot,
          4U);
    }

    TEST(SnapshotPolicyTest, ExplicitRequestHasPriorityOverInterval)
    {
      const SnapshotPolicy policy{
          10,
          3,
          true};

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{10}},
              EventId{
                  EventIdValue{10}},
              nullptr,
              true,
              true);

      EXPECT_TRUE(
          decision.required);

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::Explicit);
    }

    TEST(SnapshotPolicyTest, RoomCloseCreatesSnapshotWhenEnabled)
    {
      const SnapshotPolicy policy{
          100,
          3,
          true};

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{4}},
              EventId{
                  EventIdValue{4}},
              nullptr,
              true,
              false);

      EXPECT_TRUE(
          decision.required);

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::RoomClose);

      EXPECT_EQ(
          decision.eventsSinceSnapshot,
          4U);
    }

    TEST(SnapshotPolicyTest, RoomCloseDoesNotCreateSnapshotWhenDisabled)
    {
      const SnapshotPolicy policy{
          100,
          3,
          false};

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{4}},
              EventId{
                  EventIdValue{4}},
              nullptr,
              true,
              false);

      EXPECT_FALSE(
          decision.required);

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::None);
    }

    TEST(SnapshotPolicyTest, RoomCloseDoesNotDuplicateLatestPosition)
    {
      const SnapshotPolicy policy{
          100,
          3,
          true};

      const RoomSnapshot latest =
          make_snapshot(
              VersionValue{4},
              EventIdValue{4});

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{4}},
              EventId{
                  EventIdValue{4}},
              &latest,
              true,
              false);

      EXPECT_FALSE(
          decision.required);

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::None);

      EXPECT_EQ(
          decision.eventsSinceSnapshot,
          0U);
    }

    TEST(SnapshotPolicyTest, DisabledIntervalStillAllowsExplicitSnapshot)
    {
      const SnapshotPolicy policy{
          0,
          3,
          false};

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{5}},
              EventId{
                  EventIdValue{5}},
              nullptr,
              false,
              true);

      EXPECT_TRUE(
          decision.required);

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::Explicit);
    }

    TEST(SnapshotPolicyTest, DisabledIntervalDoesNotCreateAutomaticSnapshot)
    {
      const SnapshotPolicy policy{
          0,
          3,
          false};

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{100}},
              EventId{
                  EventIdValue{100}});

      EXPECT_FALSE(
          decision.required);

      EXPECT_EQ(
          decision.reason,
          SnapshotReason::None);

      EXPECT_EQ(
          decision.eventsSinceSnapshot,
          100U);
    }

    TEST(SnapshotPolicyTest, DecisionCarriesRetentionCount)
    {
      const SnapshotPolicy policy{
          1,
          7,
          false};

      const SnapshotDecision decision =
          policy.evaluate(
              RoomVersion{
                  VersionValue{1}},
              EventId{
                  EventIdValue{1}});

      EXPECT_TRUE(
          decision.required);

      EXPECT_EQ(
          decision.snapshotsToKeep,
          7U);
    }

  } // namespace

} // namespace vix::realtime::internal
