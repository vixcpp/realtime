/**
 *
 * @file memory_event_store.hpp
 * @author Gaspard Kirira
 * @brief Thread-safe in-memory event store for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_MEMORY_EVENT_STORE_HPP
#define VIX_REALTIME_MEMORY_EVENT_STORE_HPP

#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/event_store.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>

namespace vix::realtime
{
  /**
   * @brief Thread-safe event store backed by process memory.
   *
   * Events are stored in separate ordered streams for each room. Event IDs
   * begin at one and increase monotonically within each stream.
   *
   * This implementation is intended for development, tests, examples, and
   * single-process applications that do not require durable persistence.
   */
  class VIX_REALTIME_API MemoryEventStore final : public EventStore
  {
  public:
    /**
     * @brief Construct an empty in-memory event store.
     */
    MemoryEventStore() = default;

    /**
     * @brief Destroy the event store.
     */
    ~MemoryEventStore() override = default;

    /**
     * @brief Persist one event in memory.
     *
     * @param event Event to persist.
     * @return Persisted event with its assigned event ID.
     */
    [[nodiscard]] RoomEvent append(
        RoomEvent event) override;

    /**
     * @brief Persist one atomic event batch in memory.
     *
     * @param events Events belonging to the same room.
     * @return Persisted events with assigned event IDs.
     */
    [[nodiscard]] std::vector<RoomEvent> append_batch(
        std::vector<RoomEvent> events) override;

    /**
     * @brief Load ordered events after an exclusive cursor.
     *
     * @param roomId Room stream to read.
     * @param after Exclusive event cursor.
     * @param limit Maximum number of events to return.
     * @return Ordered event collection.
     */
    [[nodiscard]] std::vector<RoomEvent> load_after(
        const RoomId &roomId,
        EventId after,
        std::size_t limit) const override;

    /**
     * @brief Return the latest event identifier in a room.
     *
     * @param roomId Room stream to inspect.
     * @return Latest event ID, or an empty ID when no events exist.
     */
    [[nodiscard]] EventId latest_event_id(
        const RoomId &roomId) const override;

    /**
     * @brief Return the number of events stored for a room.
     *
     * @param roomId Room stream to inspect.
     * @return Persisted event count.
     */
    [[nodiscard]] std::size_t count(
        const RoomId &roomId) const override;

    /**
     * @brief Remove the complete event stream of a room.
     *
     * @param roomId Room stream to remove.
     * @return True when an existing stream was removed.
     */
    bool clear_room(
        const RoomId &roomId) override;

    /**
     * @brief Remove all room event streams.
     */
    void clear();

    /**
     * @brief Return the number of room streams currently stored.
     *
     * @return Room stream count.
     */
    [[nodiscard]] std::size_t room_count() const noexcept;

  private:
    /**
     * @brief Ordered events and stream position for one room.
     */
    struct Stream
    {
      /** @brief Persisted events in ascending ID order. */
      std::vector<RoomEvent> events{};

      /** @brief Latest assigned event identifier. */
      EventId lastEventId{};

      /** @brief Latest persisted room version. */
      RoomVersion lastRoomVersion{};
    };

    /**
     * @brief Validate an event against the current stream position.
     *
     * @param event Event to validate.
     * @param stream Current room stream.
     *
     * @throws vix::realtime::Error
     *         When the event ID, room version, or room identifier is invalid.
     */
    static void validate_append(
        const RoomEvent &event,
        const Stream &stream);

    /** @brief Protects every in-memory room stream. */
    mutable std::mutex mutex_{};

    /** @brief Event streams indexed by room identifier. */
    std::unordered_map<RoomId, Stream> streams_{};
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_MEMORY_EVENT_STORE_HPP
