/**
 *
 * @file local_presence_store.hpp
 * @author Gaspard Kirira
 * @brief Thread-safe process-local presence store for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_LOCAL_PRESENCE_STORE_HPP
#define VIX_REALTIME_LOCAL_PRESENCE_STORE_HPP

#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/presence.hpp>
#include <vix/realtime/presence_store.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Thread-safe presence store backed by process memory.
   *
   * Presence records are grouped by room and indexed by logical session ID.
   *
   * This implementation is intended for:
   *
   * - single-process Realtime runtimes;
   * - tests and examples;
   * - development environments;
   * - deployments that do not require distributed presence.
   */
  class VIX_REALTIME_API LocalPresenceStore final
      : public PresenceStore
  {
  public:
    /**
     * @brief Construct an empty local presence store.
     */
    LocalPresenceStore() = default;

    /**
     * @brief Destroy the local presence store.
     */
    ~LocalPresenceStore() override = default;

    /**
     * @brief Insert or replace one presence record.
     *
     * @param presence Presence record to store.
     * @return Stored presence record.
     */
    [[nodiscard]] Presence upsert(
        Presence presence) override;

    /**
     * @brief Find one stored presence record.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @return Stored presence, or no value when absent.
     */
    [[nodiscard]] std::optional<Presence> find(
        const RoomId &roomId,
        const SessionId &sessionId) const override;

    /**
     * @brief Update one presence activity timestamp.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @param now Activity timestamp.
     * @return Updated presence record.
     */
    [[nodiscard]] Presence touch(
        const RoomId &roomId,
        const SessionId &sessionId,
        Timestamp now = SystemClock::now()) override;

    /**
     * @brief Mark one presence as connected and present.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @param connectionId Active connection identifier, or empty.
     * @param nodeId Optional node reporting the presence.
     * @param now Presence update timestamp.
     * @return Updated presence record.
     */
    [[nodiscard]] Presence mark_present(
        const RoomId &roomId,
        const SessionId &sessionId,
        ConnectionId connectionId = {},
        std::optional<NodeId> nodeId = std::nullopt,
        Timestamp now = SystemClock::now()) override;

    /**
     * @brief Mark one presence as temporarily detached.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @param now Detachment timestamp.
     * @return Updated presence record.
     */
    [[nodiscard]] Presence mark_detached(
        const RoomId &roomId,
        const SessionId &sessionId,
        Timestamp now = SystemClock::now()) override;

    /**
     * @brief Mark one presence as permanently left.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @param now Leave timestamp.
     * @return Updated presence record.
     */
    [[nodiscard]] Presence mark_left(
        const RoomId &roomId,
        const SessionId &sessionId,
        Timestamp now = SystemClock::now()) override;

    /**
     * @brief List every presence stored for a room.
     *
     * @param roomId Room identifier.
     * @return Presence records sorted by session identifier.
     */
    [[nodiscard]] std::vector<Presence> list_room(
        const RoomId &roomId) const override;

    /**
     * @brief List every room presence stored for a session.
     *
     * @param sessionId Logical session identifier.
     * @return Presence records sorted by room identifier.
     */
    [[nodiscard]] std::vector<Presence> list_session(
        const SessionId &sessionId) const override;

    /**
     * @brief Remove one presence record.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @return Removed presence, or no value when absent.
     */
    [[nodiscard]] std::optional<Presence> erase(
        const RoomId &roomId,
        const SessionId &sessionId) override;

    /**
     * @brief Remove stale and left presence records.
     *
     * @param now Current timestamp.
     * @param timeout Maximum permitted inactivity duration.
     * @return Number of removed records.
     */
    std::size_t prune_stale(
        Timestamp now,
        std::chrono::milliseconds timeout) override;

    /**
     * @brief Remove expired presence records.
     *
     * Compatibility alias for prune_stale.
     *
     * @param now Current timestamp.
     * @param timeout Maximum permitted inactivity duration.
     * @return Number of removed records.
     */
    std::size_t prune_expired(
        Timestamp now,
        std::chrono::milliseconds timeout)
    {
      return prune_stale(now, timeout);
    }

    /**
     * @brief Return the number of stored presences in a room.
     *
     * @param roomId Room identifier.
     * @return Room presence count.
     */
    [[nodiscard]] std::size_t count_room(
        const RoomId &roomId) const override;

    /**
     * @brief Return the total number of stored presence records.
     *
     * @return Total presence count.
     */
    [[nodiscard]] std::size_t count() const override;

    /**
     * @brief Remove every presence belonging to a room.
     *
     * @param roomId Room identifier.
     * @return Number of removed records.
     */
    std::size_t clear_room(
        const RoomId &roomId) override;

    /**
     * @brief Remove every presence belonging to a session.
     *
     * @param sessionId Logical session identifier.
     * @return Number of removed records.
     */
    std::size_t clear_session(
        const SessionId &sessionId) override;

    /**
     * @brief Remove every stored presence record.
     */
    void clear();

    /**
     * @brief Return the number of rooms with stored presence.
     *
     * @return Non-empty room presence stream count.
     */
    [[nodiscard]] std::size_t room_count() const;

  private:
    /**
     * @brief Presence records indexed by logical session identifier.
     */
    using SessionPresenceMap =
        std::unordered_map<SessionId, Presence>;

    /**
     * @brief Validate room and session lookup identifiers.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     */
    static void validate_key(
        const RoomId &roomId,
        const SessionId &sessionId);

    /**
     * @brief Return a mutable stored presence while the store is locked.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @return Mutable presence record.
     *
     * @throws vix::realtime::Error
     *         When the presence does not exist.
     */
    [[nodiscard]] Presence &require_locked(
        const RoomId &roomId,
        const SessionId &sessionId);

    /** @brief Protects all process-local presence state. */
    mutable std::mutex mutex_{};

    /** @brief Presence collections indexed by room identifier. */
    std::unordered_map<RoomId, SessionPresenceMap> rooms_{};

    /** @brief Total number of stored presence records. */
    std::size_t count_{0};
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_LOCAL_PRESENCE_STORE_HPP
