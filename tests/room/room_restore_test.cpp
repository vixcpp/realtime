/**
 *
 * @file room_restore_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for restoring Vix Realtime rooms from persisted history.
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

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/json/json.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/memory_event_store.hpp>
#include <vix/realtime/memory_snapshot_store.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    class CounterState final : public RoomState
    {
    public:
      explicit CounterState(
          std::int64_t value = 0)
          : value_(value)
      {
      }

      [[nodiscard]] SchemaVersion
      schema_version() const noexcept override
      {
        return SchemaVersion{1};
      }

      void apply(
          const RoomEvent &event) override
      {
        const auto payload =
            vix::json::to_json(
                event.payload());

        if (event.type() ==
            "counter.incremented")
        {
          value_ +=
              payload.at("amount")
                  .get<std::int64_t>();
        }
        else if (event.type() ==
                 "counter.decremented")
        {
          value_ -=
              payload.at("amount")
                  .get<std::int64_t>();
        }
        else if (event.type() ==
                 "counter.reset")
        {
          value_ = 0;
        }
        else if (event.type() ==
                 "counter.mutate_then_fail")
        {
          value_ +=
              payload.at("amount")
                  .get<std::int64_t>();

          throw Error{
              ErrorCode::EventApplyFailure,
              "counter event failed after mutation"};
        }
        else
        {
          throw Error{
              ErrorCode::EventApplyFailure,
              "unsupported counter event"};
        }

        appliedEventIds_.push_back(
            event.event_id());
      }

      [[nodiscard]] JsonObject
      serialize() const override
      {
        JsonObject state;

        state.set_i64(
            "value",
            value_);

        return state;
      }

      void restore(
          const JsonObject &state,
          SchemaVersion schemaVersion) override
      {
        if (schemaVersion !=
            SchemaVersion{1})
        {
          throw Error{
              ErrorCode::CorruptedState,
              "unsupported counter state schema"};
        }

        const auto json =
            vix::json::to_json(
                state);

        value_ =
            json.at("value")
                .get<std::int64_t>();

        appliedEventIds_.clear();
      }

      [[nodiscard]] std::unique_ptr<RoomState>
      clone() const override
      {
        return std::make_unique<CounterState>(
            *this);
      }

      [[nodiscard]] std::int64_t
      value() const noexcept
      {
        return value_;
      }

      [[nodiscard]] const std::vector<EventId> &
      applied_event_ids() const noexcept
      {
        return appliedEventIds_;
      }

    private:
      std::int64_t value_{0};
      std::vector<EventId> appliedEventIds_{};
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

    struct RoomDependencies
    {
      std::shared_ptr<MemoryEventStore>
          eventStore{
              std::make_shared<
                  MemoryEventStore>()};

      std::shared_ptr<MemorySnapshotStore>
          snapshotStore{
              std::make_shared<
                  MemorySnapshotStore>()};
    };

    [[nodiscard]] RoomId make_room_id()
    {
      return RoomId{
          std::string_view{
              "counter/main"}};
    }

    [[nodiscard]] RoomEvent make_event(
        VersionValue version,
        std::int64_t amount,
        std::string type =
            "counter.incremented")
    {
      JsonObject payload;

      payload.set_i64(
          "amount",
          amount);

      RoomEvent event{
          make_room_id(),
          std::move(type),
          std::move(payload),
          EventAudience::Room};

      event.set_room_version(
          RoomVersion{
              version});

      return event;
    }

    [[nodiscard]] RoomSnapshot make_snapshot(
        VersionValue version,
        EventIdValue eventId,
        std::int64_t value,
        SchemaVersion schemaVersion =
            SchemaVersion{1})
    {
      JsonObject state;

      state.set_i64(
          "value",
          value);

      return RoomSnapshot{
          make_room_id(),
          RoomVersion{
              version},
          EventId{
              eventId},
          std::move(state),
          schemaVersion};
    }

    [[nodiscard]] std::unique_ptr<Room>
    make_room(
        const RoomDependencies &dependencies,
        std::int64_t initialValue = 0,
        bool restoreOnOpen = true)
    {
      Config config;

      config.restoreRoomsOnOpen =
          restoreOnOpen;

      return std::make_unique<Room>(
          make_room_id(),
          std::make_unique<CounterState>(
              initialValue),
          std::make_unique<EmptyHandler>(),
          dependencies.eventStore,
          dependencies.snapshotStore,
          std::move(config));
    }

    [[nodiscard]] const CounterState &
    counter_state(
        const Room &room)
    {
      const auto *state =
          dynamic_cast<const CounterState *>(
              &room.state());

      EXPECT_NE(
          state,
          nullptr);

      return *state;
    }

    TEST(RoomRestoreTest, RestoresStateFromLatestSnapshot)
    {
      RoomDependencies dependencies;

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{3},
              EventIdValue{3},
              42));

      auto room =
          make_room(
              dependencies);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{42});

      EXPECT_EQ(
          room->version()
              .value(),
          VersionValue{3});

      EXPECT_EQ(
          room->last_event_id()
              .value(),
          EventIdValue{3});

      EXPECT_TRUE(
          room->is_open());
    }

    TEST(RoomRestoreTest, UsesNewestAvailableSnapshot)
    {
      RoomDependencies dependencies;

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{2},
              EventIdValue{2},
              20));

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{4},
              EventIdValue{4},
              40));

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{6},
              EventIdValue{6},
              60));

      auto room =
          make_room(
              dependencies);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{60});

      EXPECT_EQ(
          room->version()
              .value(),
          VersionValue{6});

      EXPECT_EQ(
          room->last_event_id()
              .value(),
          EventIdValue{6});
    }

    TEST(RoomRestoreTest, ReplaysEventsAfterSnapshot)
    {
      RoomDependencies dependencies;

      dependencies.eventStore->append(
          make_event(
              VersionValue{1},
              5));

      dependencies.eventStore->append(
          make_event(
              VersionValue{2},
              3));

      dependencies.eventStore->append(
          make_event(
              VersionValue{3},
              4));

      dependencies.eventStore->append(
          make_event(
              VersionValue{4},
              2,
              "counter.decremented"));

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{2},
              EventIdValue{2},
              8));

      auto room =
          make_room(
              dependencies);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{10});

      EXPECT_EQ(
          room->version()
              .value(),
          VersionValue{4});

      EXPECT_EQ(
          room->last_event_id()
              .value(),
          EventIdValue{4});
    }

    TEST(RoomRestoreTest, DoesNotReplayEventsIncludedInSnapshot)
    {
      RoomDependencies dependencies;

      dependencies.eventStore->append(
          make_event(
              VersionValue{1},
              5));

      dependencies.eventStore->append(
          make_event(
              VersionValue{2},
              3));

      dependencies.eventStore->append(
          make_event(
              VersionValue{3},
              4));

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{2},
              EventIdValue{2},
              100));

      auto room =
          make_room(
              dependencies);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{104});

      const auto &applied =
          counter_state(
              *room)
              .applied_event_ids();

      ASSERT_EQ(
          applied.size(),
          1U);

      EXPECT_EQ(
          applied.front()
              .value(),
          EventIdValue{3});
    }

    TEST(RoomRestoreTest, ReplaysEntireHistoryWithoutSnapshot)
    {
      RoomDependencies dependencies;

      dependencies.eventStore->append(
          make_event(
              VersionValue{1},
              10));

      dependencies.eventStore->append(
          make_event(
              VersionValue{2},
              3,
              "counter.decremented"));

      dependencies.eventStore->append(
          make_event(
              VersionValue{3},
              4));

      auto room =
          make_room(
              dependencies);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{11});

      EXPECT_EQ(
          room->version()
              .value(),
          VersionValue{3});

      EXPECT_EQ(
          room->last_event_id()
              .value(),
          EventIdValue{3});

      EXPECT_EQ(
          counter_state(
              *room)
              .applied_event_ids()
              .size(),
          3U);
    }

    TEST(RoomRestoreTest, ReplaysEventsInPersistenceOrder)
    {
      RoomDependencies dependencies;

      dependencies.eventStore->append(
          make_event(
              VersionValue{1},
              10));

      dependencies.eventStore->append(
          make_event(
              VersionValue{2},
              0,
              "counter.reset"));

      dependencies.eventStore->append(
          make_event(
              VersionValue{3},
              3));

      auto room =
          make_room(
              dependencies);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{3});

      const auto &applied =
          counter_state(
              *room)
              .applied_event_ids();

      ASSERT_EQ(
          applied.size(),
          3U);

      EXPECT_EQ(
          applied[0].value(),
          EventIdValue{1});

      EXPECT_EQ(
          applied[1].value(),
          EventIdValue{2});

      EXPECT_EQ(
          applied[2].value(),
          EventIdValue{3});
    }

    TEST(RoomRestoreTest, SnapshotAtLatestPositionRequiresNoReplay)
    {
      RoomDependencies dependencies;

      dependencies.eventStore->append(
          make_event(
              VersionValue{1},
              5));

      dependencies.eventStore->append(
          make_event(
              VersionValue{2},
              3));

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{2},
              EventIdValue{2},
              8));

      auto room =
          make_room(
              dependencies);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{8});

      EXPECT_TRUE(
          counter_state(
              *room)
              .applied_event_ids()
              .empty());

      EXPECT_EQ(
          room->version()
              .value(),
          VersionValue{2});

      EXPECT_EQ(
          room->last_event_id()
              .value(),
          EventIdValue{2});
    }

    TEST(RoomRestoreTest, RestorationDoesNotDuplicateStoredEvents)
    {
      RoomDependencies dependencies;

      dependencies.eventStore->append(
          make_event(
              VersionValue{1},
              5));

      dependencies.eventStore->append(
          make_event(
              VersionValue{2},
              3));

      ASSERT_EQ(
          dependencies.eventStore
              ->count(
                  make_room_id()),
          2U);

      auto room =
          make_room(
              dependencies);

      room->open();

      EXPECT_EQ(
          dependencies.eventStore
              ->count(
                  make_room_id()),
          2U);

      EXPECT_EQ(
          dependencies.eventStore
              ->latest_event_id(
                  make_room_id())
              .value(),
          EventIdValue{2});
    }

    TEST(RoomRestoreTest, MultipleRoomInstancesRestoreDeterministically)
    {
      RoomDependencies dependencies;

      dependencies.eventStore->append(
          make_event(
              VersionValue{1},
              5));

      dependencies.eventStore->append(
          make_event(
              VersionValue{2},
              3));

      dependencies.eventStore->append(
          make_event(
              VersionValue{3},
              2,
              "counter.decremented"));

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{1},
              EventIdValue{1},
              5));

      auto first =
          make_room(
              dependencies);

      auto second =
          make_room(
              dependencies);

      first->open();
      second->open();

      EXPECT_EQ(
          counter_state(
              *first)
              .value(),
          std::int64_t{6});

      EXPECT_EQ(
          counter_state(
              *second)
              .value(),
          std::int64_t{6});

      EXPECT_EQ(
          first->version(),
          second->version());

      EXPECT_EQ(
          first->last_event_id(),
          second->last_event_id());
    }

    TEST(RoomRestoreTest, CanDisableRestoration)
    {
      RoomDependencies dependencies;

      dependencies.eventStore->append(
          make_event(
              VersionValue{1},
              5));

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{1},
              EventIdValue{1},
              5));

      auto room =
          make_room(
              dependencies,
              42,
              false);

      room->open();

      EXPECT_EQ(
          counter_state(
              *room)
              .value(),
          std::int64_t{42});

      EXPECT_TRUE(
          room->version()
              .is_initial());

      EXPECT_TRUE(
          room->last_event_id()
              .empty());
    }

    TEST(RoomRestoreTest, InvalidSnapshotSchemaFailsRestoration)
    {
      RoomDependencies dependencies;

      dependencies.snapshotStore->save(
          make_snapshot(
              VersionValue{1},
              EventIdValue{1},
              42,
              SchemaVersion{2}));

      auto room =
          make_room(
              dependencies);

      EXPECT_THROW(
          room->open(),
          Error);

      EXPECT_EQ(
          room->status(),
          RoomStatus::Failed);

      EXPECT_FALSE(
          room->is_open());
    }

    TEST(RoomRestoreTest, ApplyFailureRollsBackRestoredState)
    {
      RoomDependencies dependencies;

      static_cast<void>(
          dependencies.eventStore->append(
              make_event(
                  VersionValue{1},
                  5)));

      static_cast<void>(
          dependencies.eventStore->append(
              make_event(
                  VersionValue{2},
                  3,
                  "counter.mutate_then_fail")));

      auto room =
          make_room(
              dependencies,
              7);

      EXPECT_THROW(
          static_cast<void>(room->open()),
          Error);

      EXPECT_EQ(room->status(), RoomStatus::Failed);
      EXPECT_EQ(counter_state(*room).value(), std::int64_t{7});
      EXPECT_TRUE(room->version().is_initial());
      EXPECT_TRUE(room->last_event_id().empty());
      EXPECT_TRUE(counter_state(*room).applied_event_ids().empty());
    }

    TEST(RoomRestoreTest, UnsupportedReplayEventFailsRestoration)
    {
      RoomDependencies dependencies;

      dependencies.eventStore->append(
          make_event(
              VersionValue{1},
              5,
              "counter.unknown"));

      auto room =
          make_room(
              dependencies);

      EXPECT_THROW(
          room->open(),
          Error);

      EXPECT_EQ(
          room->status(),
          RoomStatus::Failed);

      EXPECT_FALSE(
          room->is_open());
    }

  } // namespace

} // namespace vix::realtime
