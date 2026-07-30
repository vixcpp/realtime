/**
 *
 * @file room_state_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime room state interface.
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

#include <vix/json/json.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    class TestRoomState final : public RoomState
    {
    public:
      explicit TestRoomState(
          std::int64_t value = 0,
          SchemaVersion schemaVersion = 1)
          : value_(value),
            schemaVersion_(schemaVersion)
      {
      }

      [[nodiscard]] SchemaVersion
      schema_version() const noexcept override
      {
        return schemaVersion_;
      }

      void apply(
          const RoomEvent &event) override
      {
        lastEventType_ = event.type();

        if (event.type() == "counter.incremented")
        {
          ++value_;
        }
        else if (event.type() == "counter.decremented")
        {
          --value_;
        }
      }

      [[nodiscard]] JsonObject
      serialize() const override
      {
        JsonObject state;

        state.set_i64(
            "value",
            value_);

        state.set_string(
            "last_event_type",
            lastEventType_);

        return state;
      }

      void restore(
          const JsonObject &state,
          SchemaVersion schemaVersion) override
      {
        const auto json =
            vix::json::to_json(state);

        value_ =
            json.at("value")
                .get<std::int64_t>();

        lastEventType_ =
            json.at("last_event_type")
                .get<std::string>();

        schemaVersion_ =
            schemaVersion;
      }

      [[nodiscard]] std::unique_ptr<RoomState>
      clone() const override
      {
        return std::make_unique<TestRoomState>(
            *this);
      }

      [[nodiscard]] std::int64_t
      value() const noexcept
      {
        return value_;
      }

      [[nodiscard]] const std::string &
      last_event_type() const noexcept
      {
        return lastEventType_;
      }

    private:
      std::int64_t value_{0};
      SchemaVersion schemaVersion_{1};
      std::string lastEventType_{};
    };

    [[nodiscard]] RoomEvent make_event(
        std::string type)
    {
      return RoomEvent{
          RoomId{
              std::string_view{
                  "counter/main"}},
          std::move(type),
          {},
          EventAudience::Room};
    }

    TEST(RoomStateTest, ExposesSchemaVersion)
    {
      const TestRoomState state{
          0,
          SchemaVersion{3}};

      EXPECT_EQ(
          state.schema_version(),
          SchemaVersion{3});
    }

    TEST(RoomStateTest, CanBeUsedThroughBaseInterface)
    {
      std::unique_ptr<RoomState> state =
          std::make_unique<TestRoomState>(
              7,
              SchemaVersion{2});

      EXPECT_EQ(
          state->schema_version(),
          SchemaVersion{2});

      const auto serialized =
          vix::json::to_json(
              state->serialize());

      EXPECT_EQ(
          serialized.at("value")
              .get<std::int64_t>(),
          std::int64_t{7});
    }

    TEST(RoomStateTest, AppliesRoomEvent)
    {
      TestRoomState state{
          7};

      state.apply(
          make_event(
              "counter.incremented"));

      EXPECT_EQ(
          state.value(),
          std::int64_t{8});

      EXPECT_EQ(
          state.last_event_type(),
          "counter.incremented");
    }

    TEST(RoomStateTest, AppliesEventsInOrder)
    {
      TestRoomState state{
          10};

      state.apply(
          make_event(
              "counter.incremented"));

      state.apply(
          make_event(
              "counter.incremented"));

      state.apply(
          make_event(
              "counter.decremented"));

      EXPECT_EQ(
          state.value(),
          std::int64_t{11});

      EXPECT_EQ(
          state.last_event_type(),
          "counter.decremented");
    }

    TEST(RoomStateTest, IgnoresUnknownEventWithoutChangingValue)
    {
      TestRoomState state{
          12};

      state.apply(
          make_event(
              "counter.observed"));

      EXPECT_EQ(
          state.value(),
          std::int64_t{12});

      EXPECT_EQ(
          state.last_event_type(),
          "counter.observed");
    }

    TEST(RoomStateTest, SerializesState)
    {
      TestRoomState state{
          42};

      state.apply(
          make_event(
              "counter.incremented"));

      const auto serialized =
          vix::json::to_json(
              state.serialize());

      EXPECT_EQ(
          serialized.at("value")
              .get<std::int64_t>(),
          std::int64_t{43});

      EXPECT_EQ(
          serialized.at("last_event_type")
              .get<std::string>(),
          "counter.incremented");
    }

    TEST(RoomStateTest, RestoresSerializedState)
    {
      JsonObject serialized;

      serialized.set_i64(
          "value",
          84);

      serialized.set_string(
          "last_event_type",
          "counter.restored");

      TestRoomState state;

      state.restore(
          serialized,
          SchemaVersion{4});

      EXPECT_EQ(
          state.value(),
          std::int64_t{84});

      EXPECT_EQ(
          state.last_event_type(),
          "counter.restored");

      EXPECT_EQ(
          state.schema_version(),
          SchemaVersion{4});
    }

    TEST(RoomStateTest, SerializationCanBeRestored)
    {
      TestRoomState original{
          41,
          SchemaVersion{2}};

      original.apply(
          make_event(
              "counter.incremented"));

      TestRoomState restored;

      restored.restore(
          original.serialize(),
          original.schema_version());

      EXPECT_EQ(
          restored.value(),
          original.value());

      EXPECT_EQ(
          restored.last_event_type(),
          original.last_event_type());

      EXPECT_EQ(
          restored.schema_version(),
          original.schema_version());
    }

    TEST(RoomStateTest, ClonePreservesState)
    {
      TestRoomState original{
          42,
          SchemaVersion{3}};

      original.apply(
          make_event(
              "counter.incremented"));

      std::unique_ptr<RoomState> clone =
          original.clone();

      ASSERT_NE(
          clone,
          nullptr);

      EXPECT_NE(
          clone.get(),
          &original);

      EXPECT_EQ(
          clone->schema_version(),
          SchemaVersion{3});

      EXPECT_EQ(
          vix::json::to_json(
              clone->serialize()),
          vix::json::to_json(
              original.serialize()));
    }

    TEST(RoomStateTest, CloneIsIndependentFromOriginal)
    {
      TestRoomState original{
          10};

      std::unique_ptr<RoomState> clone =
          original.clone();

      original.apply(
          make_event(
              "counter.incremented"));

      const auto originalJson =
          vix::json::to_json(
              original.serialize());

      const auto cloneJson =
          vix::json::to_json(
              clone->serialize());

      EXPECT_EQ(
          originalJson.at("value")
              .get<std::int64_t>(),
          std::int64_t{11});

      EXPECT_EQ(
          cloneJson.at("value")
              .get<std::int64_t>(),
          std::int64_t{10});
    }

  } // namespace

} // namespace vix::realtime
