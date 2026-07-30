/**
 *
 * @file presence_store.hpp
 * @author Gaspard Kirira
 * @brief Presence storage interface for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_PRESENCE_STORE_HPP
#define VIX_REALTIME_PRESENCE_STORE_HPP

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/presence.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Storage contract for logical room presence records.
   *
   * Presence is uniquely identified by the pair:
   *
   * - room identifier;
   * - logical session identifier.
   *
   * Implementations must provide atomic updates for touch, reconnection,
   * detachment, and leave transitions.
   */
  class VIX_REALTIME_API PresenceStore
  {
  public:
    /**
     * @brief Destroy the presence store.
     */
    virtual ~PresenceStore() = default;

    /**
     * @brief Insert or replace one presence record.
     *
     * The replacement must not move membership timestamps backwards.
     *
     * @param presence Presence record to store.
     * @return Stored presence record.
     *
     * @throws vix::realtime::Error
     *         When the presence is invalid or older than the stored record.
     */
    [[nodiscard]] virtual Presence upsert(
        Presence presence) = 0;

    /**
     * @brief Find one room presence.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @return Stored presence, or no value when absent.
     */
    [[nodiscard]] virtual std::optional<Presence> find(
        const RoomId &roomId,
        const SessionId &sessionId) const = 0;

    /**
     * @brief Update the latest activity timestamp.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @param now Activity timestamp.
     * @return Updated presence record.
     *
     * @throws vix::realtime::Error
     *         When the presence does not exist or cannot be updated.
     */
    [[nodiscard]] virtual Presence touch(
        const RoomId &roomId,
        const SessionId &sessionId,
        Timestamp now = SystemClock::now()) = 0;

    /**
     * @brief Mark one room presence as connected and present.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @param connectionId Active connection identifier, or an empty string.
     * @param nodeId Optional node reporting the presence.
     * @param now Presence update timestamp.
     * @return Updated presence record.
     *
     * @throws vix::realtime::Error
     *         When the presence does not exist or cannot become present.
     */
    [[nodiscard]] virtual Presence mark_present(
        const RoomId &roomId,
        const SessionId &sessionId,
        ConnectionId connectionId = {},
        std::optional<NodeId> nodeId = std::nullopt,
        Timestamp now = SystemClock::now()) = 0;

    /**
     * @brief Mark one room presence as temporarily detached.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @param now Detachment timestamp.
     * @return Updated presence record.
     *
     * @throws vix::realtime::Error
     *         When the presence does not exist or cannot be detached.
     */
    [[nodiscard]] virtual Presence mark_detached(
        const RoomId &roomId,
        const SessionId &sessionId,
        Timestamp now = SystemClock::now()) = 0;

    /**
     * @brief Mark one room presence as permanently left.
     *
     * The record remains stored until explicitly removed or pruned.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @param now Leave timestamp.
     * @return Updated presence record.
     *
     * @throws vix::realtime::Error
     *         When the presence does not exist or cannot be updated.
     */
    [[nodiscard]] virtual Presence mark_left(
        const RoomId &roomId,
        const SessionId &sessionId,
        Timestamp now = SystemClock::now()) = 0;

    /**
     * @brief List every stored presence in a room.
     *
     * Results must be returned in deterministic session identifier order.
     *
     * @param roomId Room identifier.
     * @return Stored room presence records.
     */
    [[nodiscard]] virtual std::vector<Presence> list_room(
        const RoomId &roomId) const = 0;

    /**
     * @brief List every stored room presence for a session.
     *
     * Results must be returned in deterministic room identifier order.
     *
     * @param sessionId Logical session identifier.
     * @return Presence records belonging to the session.
     */
    [[nodiscard]] virtual std::vector<Presence> list_session(
        const SessionId &sessionId) const = 0;

    /**
     * @brief Remove one presence record.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @return Removed presence, or no value when absent.
     */
    [[nodiscard]] virtual std::optional<Presence> erase(
        const RoomId &roomId,
        const SessionId &sessionId) = 0;

    /**
     * @brief Remove stale and permanently left presence records.
     *
     * @param now Current timestamp.
     * @param timeout Maximum permitted inactivity duration.
     * @return Number of removed presence records.
     *
     * @throws vix::realtime::Error
     *         When the timeout is negative.
     */
    virtual std::size_t prune_stale(
        Timestamp now,
        std::chrono::milliseconds timeout) = 0;

    /**
     * @brief Return the number of presence records stored for a room.
     *
     * @param roomId Room identifier.
     * @return Stored room presence count.
     */
    [[nodiscard]] virtual std::size_t count_room(
        const RoomId &roomId) const = 0;

    /**
     * @brief Return the total number of stored presence records.
     *
     * @return Total presence count.
     */
    [[nodiscard]] virtual std::size_t count() const = 0;

    /**
     * @brief Remove every presence record belonging to a room.
     *
     * @param roomId Room identifier.
     * @return Number of removed presence records.
     */
    virtual std::size_t clear_room(
        const RoomId &roomId) = 0;

    /**
     * @brief Remove every presence record belonging to a session.
     *
     * @param sessionId Logical session identifier.
     * @return Number of removed presence records.
     */
    virtual std::size_t clear_session(
        const SessionId &sessionId) = 0;
  };

  /**
   * @brief Shared ownership pointer for a presence store.
   */
  using PresenceStorePtr = std::shared_ptr<PresenceStore>;

} // namespace vix::realtime

#endif // VIX_REALTIME_PRESENCE_STORE_HPP
