/**
 *
 * @file event_apply_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for deterministic application of Vix Realtime room events.
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
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>
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

        if (event.type() == "counter.incremented")
        {
          value_ +=
              payload.at("amount")
                  .get<std::int64_t>();
        }
        else if (event.type() == "counter.decremented")
        {
          value_ -=
              payload.at("amount")
                  .get<std::int64_t>();
        }
        else if (event.type() == "counter.reset")
        {
          value_ = 0;
        }
        else
        {
          throw Error{
              ErrorCode::EventApplyFailure,
              "unsupported counter event"};
        }

        lastEventId_ =
            event.event_id();

        roomVersion_ =
            event.room_version();

        appliedTypes_.push_back(
            event.type());
      }

      [[nodiscard]] JsonObject
      serialize() const override
      {
        JsonObject state;

        state.set_i64(
            "value",
            value_);

        state.set_i64(
            "room_version",
            roomVersion_.value());

        state.set_i64(
            "last_event_id",
            lastEventId_.value());

        return state;
      }

      void restore(
          const JsonObject &state,
          SchemaVersion schemaVersion) override
      {
        if (schemaVersion != SchemaVersion{1})
        {
          throw Error{
              ErrorCode::CorruptedState,
              "unsupported counter state schema"};
        }

        const auto json =
            vix::json::to_json(state);

        value_ =
            json.at("value")
                .get<std::int64_t>();

        roomVersion_ =
            RoomVersion{
                json.at("room_version")
                    .get<VersionValue>()};

        lastEventId_ =
            EventId{
                json.at("last_event_id")
                    .get<EventIdValue>()};

        appliedTypes_.clear();
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

      [[nodiscard]] const RoomVersion &
      room_version() const noexcept
      {
        return roomVersion_;
      }

      [[nodiscard]] const EventId &
      last_event_id() const noexcept
      {
        return lastEventId_;
      }

      [[nodiscard]] const std::vector<std::string> &
      applied_types() const noexcept
      {
        return appliedTypes_;
      }

    private:
      std::int64_t value_{0};
      RoomVersion roomVersion_{};
      EventId lastEventId_{};
      std::vector<std::string> appliedTypes_{};
    };

    [[nodiscard]] RoomId make_room_id()
    {
      return RoomId{
          std::string_view{
              "counter/main"}};
    }

    [[nodiscard]] RoomEvent make_event(
        std::string type,
        std::int64_t amount,
        EventIdValue eventId,
        VersionValue roomVersion)
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

      event
          .set_event_id(
              EventId{
                  eventId})
          .set_room_version(
              RoomVersion{
                  roomVersion});

      return event;
    }

    TEST(EventApplyTest, AppliesIncrementEvent)
    {
      CounterState state{
          10};

      state.apply(
          make_event(
              "counter.incremented",
              3,
              EventIdValue{1},
              VersionValue{1}));

      EXPECT_EQ(
          state.value(),
          std::int64_t{13});

      EXPECT_EQ(
          state.last_event_id()
              .value(),
          EventIdValue{1});

      EXPECT_EQ(
          state.room_version()
              .value(),
          VersionValue{1});
    }

    TEST(EventApplyTest, AppliesDecrementEvent)
    {
      CounterState state{
          10};

      state.apply(
          make_event(
              "counter.decremented",
              4,
              EventIdValue{1},
              VersionValue{1}));

      EXPECT_EQ(
          state.value(),
          std::int64_t{6});
    }

    TEST(EventApplyTest, AppliesResetEvent)
    {
      CounterState state{
          42};

      state.apply(
          make_event(
              "counter.reset",
              0,
              EventIdValue{1},
              VersionValue{1}));

      EXPECT_EQ(
          state.value(),
          std::int64_t{0});
    }

    TEST(EventApplyTest, AppliesEventsInPersistedOrder)
    {
      CounterState state;

      const std::vector<RoomEvent> events{
          make_event(
              "counter.incremented",
              10,
              EventIdValue{1},
              VersionValue{1}),
          make_event(
              "counter.incremented",
              5,
              EventIdValue{2},
              VersionValue{2}),
          make_event(
              "counter.decremented",
              3,
              EventIdValue{3},
              VersionValue{3})};

      for (const RoomEvent &event : events)
      {
        state.apply(event);
      }

      EXPECT_EQ(
          state.value(),
          std::int64_t{12});

      EXPECT_EQ(
          state.room_version()
              .value(),
          VersionValue{3});

      EXPECT_EQ(
          state.last_event_id()
              .value(),
          EventIdValue{3});

      ASSERT_EQ(
          state.applied_types().size(),
          3U);

      EXPECT_EQ(
          state.applied_types()[0],
          "counter.incremented");

      EXPECT_EQ(
          state.applied_types()[1],
          "counter.incremented");

      EXPECT_EQ(
          state.applied_types()[2],
          "counter.decremented");
    }

    TEST(EventApplyTest, EventOrderChangesFinalState)
    {
      CounterState resetLast;
      CounterState resetFirst;

      const RoomEvent increment =
          make_event(
              "counter.incremented",
              10,
              EventIdValue{1},
              VersionValue{1});

      const RoomEvent reset =
          make_event(
              "counter.reset",
              0,
              EventIdValue{2},
              VersionValue{2});

      resetLast.apply(increment);
      resetLast.apply(reset);

      resetFirst.apply(reset);
      resetFirst.apply(increment);

      EXPECT_EQ(
          resetLast.value(),
          std::int64_t{0});

      EXPECT_EQ(
          resetFirst.value(),
          std::int64_t{10});
    }

    TEST(EventApplyTest, SameEventSequenceProducesSameState)
    {
      const std::vector<RoomEvent> events{
          make_event(
              "counter.incremented",
              10,
              EventIdValue{1},
              VersionValue{1}),
          make_event(
              "counter.decremented",
              4,
              EventIdValue{2},
              VersionValue{2}),
          make_event(
              "counter.incremented",
              7,
              EventIdValue{3},
              VersionValue{3})};

      CounterState first;
      CounterState second;

      for (const RoomEvent &event : events)
      {
        first.apply(event);
        second.apply(event);
      }

      EXPECT_EQ(
          first.value(),
          second.value());

      EXPECT_EQ(
          first.room_version(),
          second.room_version());

      EXPECT_EQ(
          first.last_event_id(),
          second.last_event_id());

      EXPECT_EQ(
          vix::json::to_json(
              first.serialize()),
          vix::json::to_json(
              second.serialize()));
    }

    TEST(EventApplyTest, RejectsUnknownEventType)
    {
      CounterState state{
          10};

      EXPECT_THROW(
          state.apply(
              make_event(
                  "counter.unknown",
                  3,
                  EventIdValue{1},
                  VersionValue{1})),
          Error);

      EXPECT_EQ(
          state.value(),
          std::int64_t{10});

      EXPECT_TRUE(
          state.last_event_id()
              .empty());

      EXPECT_TRUE(
          state.room_version()
              .is_initial());
    }

    TEST(EventApplyTest, FailedEventDoesNotRecordApplication)
    {
      CounterState state{
          10};

      EXPECT_THROW(
          state.apply(
              make_event(
                  "counter.unknown",
                  3,
                  EventIdValue{1},
                  VersionValue{1})),
          Error);

      EXPECT_TRUE(
          state.applied_types()
              .empty());
    }

    TEST(EventApplyTest, StateCanBeSerializedAndRestored)
    {
      CounterState original;

      original.apply(
          make_event(
              "counter.incremented",
              12,
              EventIdValue{1},
              VersionValue{1}));

      original.apply(
          make_event(
              "counter.decremented",
              2,
              EventIdValue{2},
              VersionValue{2}));

      CounterState restored;

      restored.restore(
          original.serialize(),
          original.schema_version());

      EXPECT_EQ(
          restored.value(),
          std::int64_t{10});

      EXPECT_EQ(
          restored.room_version()
              .value(),
          VersionValue{2});

      EXPECT_EQ(
          restored.last_event_id()
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          vix::json::to_json(
              restored.serialize()),
          vix::json::to_json(
              original.serialize()));
    }

    TEST(EventApplyTest, ReplayCanContinueAfterRestoredState)
    {
      CounterState original;

      original.apply(
          make_event(
              "counter.incremented",
              10,
              EventIdValue{1},
              VersionValue{1}));

      CounterState restored;

      restored.restore(
          original.serialize(),
          original.schema_version());

      restored.apply(
          make_event(
              "counter.incremented",
              5,
              EventIdValue{2},
              VersionValue{2}));

      EXPECT_EQ(
          restored.value(),
          std::int64_t{15});

      EXPECT_EQ(
          restored.last_event_id()
              .value(),
          EventIdValue{2});

      EXPECT_EQ(
          restored.room_version()
              .value(),
          VersionValue{2});
    }

    TEST(EventApplyTest, CloneCanReplayIndependently)
    {
      CounterState original{
          10};

      std::unique_ptr<RoomState> clonedBase =
          original.clone();

      auto *clone =
          dynamic_cast<CounterState *>(
              clonedBase.get());

      ASSERT_NE(
          clone,
          nullptr);

      clone->apply(
          make_event(
              "counter.incremented",
              5,
              EventIdValue{1},
              VersionValue{1}));

      EXPECT_EQ(
          original.value(),
          std::int64_t{10});

      EXPECT_EQ(
          clone->value(),
          std::int64_t{15});
    }

  } // namespace

} // namespace vix::realtime
