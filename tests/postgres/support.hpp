#ifndef VIX_REALTIME_TESTS_POSTGRES_SUPPORT_HPP
#define VIX_REALTIME_TESTS_POSTGRES_SUPPORT_HPP

#include <cstdlib>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include <vix/json/json.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/postgres_event_store.hpp>
#include <vix/realtime/postgres_snapshot_store.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime::postgres_test
{
  inline std::string connection_string()
  {
    const char *value = std::getenv("VIX_REALTIME_POSTGRES_TEST_URL");
    return value == nullptr ? std::string{} : std::string{value};
  }

  inline bool configured() noexcept
  {
    const char *value = std::getenv("VIX_REALTIME_POSTGRES_TEST_URL");
    return value != nullptr && *value != '\0';
  }

  inline PostgresEventStoreOptions event_options()
  {
    PostgresEventStoreOptions options;
    options.connectionString = connection_string();
    options.table = "vix_realtime_test_events";
    options.createTableIfMissing = true;
    return options;
  }

  inline PostgresSnapshotStoreOptions snapshot_options()
  {
    PostgresSnapshotStoreOptions options;
    options.connectionString = connection_string();
    options.table = "vix_realtime_test_snapshots";
    options.createTableIfMissing = true;
    return options;
  }

  inline RoomId room_id(std::string_view value)
  {
    return RoomId{std::string{value}};
  }

  inline RoomEvent event(
      const RoomId &roomId,
      VersionValue version,
      std::int64_t amount)
  {
    JsonObject payload;
    payload.set_i64("amount", amount);

    RoomEvent result{
        roomId,
        "counter.incremented",
        std::move(payload),
        EventAudience::Room};
    result.set_room_version(RoomVersion{version});
    return result;
  }

  inline RoomSnapshot snapshot(
      const RoomId &roomId,
      VersionValue version,
      EventIdValue eventId,
      std::int64_t value)
  {
    JsonObject state;
    state.set_i64("value", value);

    return RoomSnapshot{
        roomId,
        RoomVersion{version},
        EventId{eventId},
        std::move(state),
        SchemaVersion{1}};
  }
} // namespace vix::realtime::postgres_test

#define VIX_REALTIME_REQUIRE_POSTGRES_TEST_DATABASE()                     \
  do                                                                       \
  {                                                                        \
    if (!::vix::realtime::postgres_test::configured())                    \
    {                                                                      \
      GTEST_SKIP() << "VIX_REALTIME_POSTGRES_TEST_URL is not configured"; \
    }                                                                      \
  } while (false)

#endif // VIX_REALTIME_TESTS_POSTGRES_SUPPORT_HPP
