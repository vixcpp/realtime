/**
 *
 * @file event_store.hpp
 * @author Gaspard Kirira
 * @brief Persistent event stream interface for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_EVENT_STORE_HPP
#define VIX_REALTIME_EVENT_STORE_HPP

#include <cstddef>
#include <memory>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>

namespace vix::realtime
{
  /**
   * @brief Persistence contract for authoritative room event streams.
   *
   * Events are ordered independently for each room. The store assigns a
   * monotonic `EventId` when an event is appended.
   *
   * The room runtime must persist events before applying them to the
   * authoritative state or broadcasting them to connected sessions.
   */
  class VIX_REALTIME_API EventStore
  {
  public:
    /**
     * @brief Destroy the event store.
     */
    virtual ~EventStore() = default;

    /**
     * @brief Persist one event and assign its ordered identifier.
     *
     * The supplied event must not already contain a persistent event ID.
     *
     * @param event Event to persist.
     * @return Persisted event containing its assigned event ID.
     *
     * @throws vix::realtime::Error
     *         When validation or persistence fails.
     */
    [[nodiscard]] virtual RoomEvent append(
        RoomEvent event) = 0;

    /**
     * @brief Persist multiple events atomically.
     *
     * All events must belong to the same room and use contiguous room
     * versions. Either the complete batch is persisted or none of it is.
     *
     * @param events Events to persist.
     * @return Persisted events containing their assigned event IDs.
     *
     * @throws vix::realtime::Error
     *         When the batch is invalid or persistence fails.
     */
    [[nodiscard]] virtual std::vector<RoomEvent> append_batch(
        std::vector<RoomEvent> events) = 0;

    /**
     * @brief Load events occurring after a given event identifier.
     *
     * Results must be returned in ascending event ID order.
     *
     * An empty `after` identifier loads events from the beginning.
     *
     * @param roomId Room stream to read.
     * @param after Exclusive replay cursor.
     * @param limit Maximum number of events to return.
     * @return Ordered events after the cursor.
     *
     * @throws vix::realtime::Error
     *         When the stream cannot be read.
     */
    [[nodiscard]] virtual std::vector<RoomEvent> load_after(
        const RoomId &roomId,
        EventId after,
        std::size_t limit) const = 0;

    /**
     * @brief Return the latest event identifier in a room stream.
     *
     * @param roomId Room stream to inspect.
     * @return Latest event ID, or an empty ID when the stream is empty.
     *
     * @throws vix::realtime::Error
     *         When the stream cannot be inspected.
     */
    [[nodiscard]] virtual EventId latest_event_id(
        const RoomId &roomId) const = 0;

    /**
     * @brief Return the number of persisted events in a room stream.
     *
     * @param roomId Room stream to inspect.
     * @return Number of persisted events.
     *
     * @throws vix::realtime::Error
     *         When the stream cannot be inspected.
     */
    [[nodiscard]] virtual std::size_t count(
        const RoomId &roomId) const = 0;

    /**
     * @brief Remove every persisted event belonging to a room.
     *
     * This operation is intended for tests, administrative cleanup, and
     * explicit room deletion. Normal room shutdown must not clear history.
     *
     * @param roomId Room stream to remove.
     * @return True when a stream existed and was removed.
     *
     * @throws vix::realtime::Error
     *         When the stream cannot be removed.
     */
    virtual bool clear_room(
        const RoomId &roomId) = 0;
  };

  /**
   * @brief Shared ownership pointer for an event store.
   */
  using EventStorePtr = std::shared_ptr<EventStore>;

} // namespace vix::realtime

#endif // VIX_REALTIME_EVENT_STORE_HPP
