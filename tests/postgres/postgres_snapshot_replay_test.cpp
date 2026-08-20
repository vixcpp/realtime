/** @file postgres_snapshot_replay_test.cpp */

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

  TEST(PostgresSnapshotReplayTest, RestoresIdenticalStateFromSnapshotAndReplay)
  {
    VIX_REALTIME_REQUIRE_POSTGRES_TEST_DATABASE();
    auto events = std::make_shared<PostgresEventStore>(postgres_test::event_options());
    auto snapshots = std::make_shared<PostgresSnapshotStore>(postgres_test::snapshot_options());
    const RoomId roomId = postgres_test::room_id("postgres/replay/snapshot");
    static_cast<void>(events->clear_room(roomId));
    static_cast<void>(snapshots->clear_room(roomId));

    static_cast<void>(events->append(postgres_test::event(roomId, 1, 2)));
    static_cast<void>(events->append(postgres_test::event(roomId, 2, 3)));
    static_cast<void>(snapshots->save(postgres_test::snapshot(roomId, 2, 2, 5)));
    static_cast<void>(events->append(postgres_test::event(roomId, 3, 7)));

    CounterState replayOnlyState;
    internal::ReplayEngine replayOnlyEngine{events, nullptr};
    const internal::ReplayResult replayOnly =
        replayOnlyEngine.restore(roomId, replayOnlyState);

    CounterState snapshotState;
    internal::ReplayEngine snapshotEngine{events, snapshots};
    const internal::ReplayResult replay = snapshotEngine.restore(roomId, snapshotState);

    EXPECT_EQ(replayOnlyState.value(), std::int64_t{12});
    EXPECT_EQ(snapshotState.value(), replayOnlyState.value());
    EXPECT_EQ(replay.roomVersion, replayOnly.roomVersion);
    EXPECT_EQ(replay.lastEventId, replayOnly.lastEventId);
    ASSERT_TRUE(replay.snapshot.has_value());
    EXPECT_EQ(replay.roomVersion.value(), VersionValue{3});
    EXPECT_EQ(replay.lastEventId.value(), EventIdValue{3});
  }
} // namespace vix::realtime
