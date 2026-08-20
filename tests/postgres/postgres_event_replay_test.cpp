/** @file postgres_event_replay_test.cpp */

#include <gtest/gtest.h>

#include <memory>

#include <vix/json/json.hpp>
#include <vix/realtime/internal/replay_engine.hpp>

#include "support.hpp"

namespace vix::realtime
{
  namespace
  {
    class CounterState final : public RoomState
    {
    public:
      [[nodiscard]] SchemaVersion schema_version() const noexcept override
      {
        return SchemaVersion{1};
      }

      void apply(const RoomEvent &event) override
      {
        value_ += vix::json::to_json(event.payload()).at("amount")
                      .get<std::int64_t>();
      }

      [[nodiscard]] JsonObject serialize() const override
      {
        JsonObject state;
        state.set_i64("value", value_);
        return state;
      }

      void restore(const JsonObject &state, SchemaVersion version) override
      {
        if (version != SchemaVersion{1})
        {
          throw Error{ErrorCode::CorruptedState, "unsupported schema"};
        }

        value_ = vix::json::to_json(state).at("value").get<std::int64_t>();
      }

      [[nodiscard]] std::unique_ptr<RoomState> clone() const override
      {
        return std::make_unique<CounterState>(*this);
      }

      [[nodiscard]] std::int64_t value() const noexcept { return value_; }

    private:
      std::int64_t value_{0};
    };
  } // namespace

  TEST(PostgresEventReplayTest, RestoresHistoryWithoutSnapshot)
  {
    VIX_REALTIME_REQUIRE_POSTGRES_TEST_DATABASE();
    auto events = std::make_shared<PostgresEventStore>(postgres_test::event_options());
    const RoomId roomId = postgres_test::room_id("postgres/replay/no-snapshot");
    static_cast<void>(events->clear_room(roomId));
    static_cast<void>(events->append(postgres_test::event(roomId, 1, 4)));
    static_cast<void>(events->append(postgres_test::event(roomId, 2, 6)));

    CounterState state;
    internal::ReplayEngine engine{events, nullptr};
    const internal::ReplayResult replay = engine.restore(roomId, state);

    EXPECT_EQ(state.value(), std::int64_t{10});
    EXPECT_EQ(replay.roomVersion.value(), VersionValue{2});
    EXPECT_EQ(replay.lastEventId.value(), EventIdValue{2});
  }
} // namespace vix::realtime
