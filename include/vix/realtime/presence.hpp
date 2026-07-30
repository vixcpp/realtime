/**
 *
 * @file presence.hpp
 * @author Gaspard Kirira
 * @brief Logical room presence record for Vix Realtime sessions.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_PRESENCE_HPP
#define VIX_REALTIME_PRESENCE_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

#include <vix/realtime/api.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Lifecycle state of one logical room presence.
   */
  enum class PresenceStatus : std::uint8_t
  {
    /**
     * @brief The logical session is currently present in the room.
     *
     * A present session may optionally expose an active transport connection.
     */
    Present = 0,

    /**
     * @brief The session remains logically present but lost its connection.
     *
     * Detached presence may remain resumable until its timeout expires.
     */
    Detached,

    /**
     * @brief The logical session permanently left the room.
     */
    Left
  };

  /**
   * @brief Return the stable textual representation of a presence status.
   *
   * @param status Presence lifecycle status.
   * @return Stable lowercase identifier.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(PresenceStatus status) noexcept
  {
    switch (status)
    {
    case PresenceStatus::Present:
      return "present";

    case PresenceStatus::Detached:
      return "detached";

    case PresenceStatus::Left:
      return "left";
    }

    return "left";
  }

  /**
   * @brief Logical membership presence of one session inside one room.
   *
   * A presence record is independent from the transport connection itself.
   * It may survive a temporary disconnect and later return to `Present` when
   * the logical session resumes.
   *
   * Presence is identified by the pair:
   *
   * - `room_id()`;
   * - `session_id()`.
   *
   * The record is intentionally copyable so presence stores may return safe
   * snapshots without exposing their internal synchronization.
   */
  class VIX_REALTIME_API Presence
  {
  public:
    /**
     * @brief Construct an empty presence record.
     *
     * The resulting record is invalid until replaced with a complete value.
     */
    Presence() = default;

    /**
     * @brief Construct a present logical room membership.
     *
     * The connection identifier may be empty when presence is managed without
     * a transport-specific connection.
     *
     * @param roomId Room containing the session.
     * @param sessionId Logical session present in the room.
     * @param identity Application-defined authenticated identity.
     * @param nodeId Optional runtime node reporting the presence.
     * @param connectionId Optional active transport connection identifier.
     * @param joinedAt Time at which the session joined the room.
     * @param metadata Non-authoritative application presence metadata.
     *
     * @throws vix::realtime::Error
     *         When identifiers or timestamps are invalid.
     */
    Presence(
        RoomId roomId,
        SessionId sessionId,
        Identity identity = {},
        std::optional<NodeId> nodeId = std::nullopt,
        ConnectionId connectionId = {},
        Timestamp joinedAt = SystemClock::now(),
        JsonObject metadata = {});

    /**
     * @brief Return the room containing the session.
     *
     * @return Room identifier.
     */
    [[nodiscard]] const RoomId &room_id() const noexcept;

    /**
     * @brief Return the logical session represented by this presence.
     *
     * @return Session identifier.
     */
    [[nodiscard]] const SessionId &session_id() const noexcept;

    /**
     * @brief Return the application-defined authenticated identity.
     *
     * @return Client identity, or an empty string when anonymous.
     */
    [[nodiscard]] const Identity &identity() const noexcept;

    /**
     * @brief Return the node currently reporting the presence.
     *
     * @return Runtime node identifier, when present.
     */
    [[nodiscard]] const std::optional<NodeId> &
    node_id() const noexcept;

    /**
     * @brief Return the active transport connection identifier.
     *
     * @return Connection identifier, or an empty string when none is attached.
     */
    [[nodiscard]] const ConnectionId &
    connection_id() const noexcept;

    /**
     * @brief Return the current presence lifecycle status.
     *
     * @return Presence status.
     */
    [[nodiscard]] PresenceStatus status() const noexcept;

    /**
     * @brief Return the room join timestamp.
     *
     * @return Join timestamp.
     */
    [[nodiscard]] Timestamp joined_at() const noexcept;

    /**
     * @brief Return the latest observed activity timestamp.
     *
     * @return Latest presence activity timestamp.
     */
    [[nodiscard]] Timestamp last_seen_at() const noexcept;

    /**
     * @brief Return the latest transport detachment timestamp.
     *
     * @return Detachment timestamp, when detached.
     */
    [[nodiscard]] const std::optional<Timestamp> &
    detached_at() const noexcept;

    /**
     * @brief Return the permanent room leave timestamp.
     *
     * @return Leave timestamp, when the presence is left.
     */
    [[nodiscard]] const std::optional<Timestamp> &
    left_at() const noexcept;

    /**
     * @brief Return whether the session is logically present.
     *
     * Detached sessions remain logically present until they leave or expire.
     *
     * @return True when status is `Present` or `Detached`.
     */
    [[nodiscard]] bool logically_present() const noexcept;

    /**
     * @brief Return whether the presence exposes an active connection.
     *
     * @return True when status is `Present` and the connection ID is non-empty.
     */
    [[nodiscard]] bool connected() const noexcept;

    /**
     * @brief Return whether the presence is detached.
     *
     * @return True when the status is `Detached`.
     */
    [[nodiscard]] bool detached() const noexcept;

    /**
     * @brief Return whether the session permanently left the room.
     *
     * @return True when the status is `Left`.
     */
    [[nodiscard]] bool left() const noexcept;

    /**
     * @brief Return whether the presence exceeded its activity timeout.
     *
     * A left presence is always stale. A negative timeout is also considered
     * stale. Clock movement before `last_seen_at()` does not expire presence.
     *
     * @param now Current timestamp.
     * @param timeout Maximum permitted inactivity duration.
     * @return True when the presence should no longer be considered active.
     */
    [[nodiscard]] bool stale(
        Timestamp now,
        std::chrono::milliseconds timeout) const noexcept;

    /**
     * @brief Return whether the presence is currently active.
     *
     * @param now Current timestamp.
     * @param timeout Maximum permitted inactivity duration.
     * @return True when logically present and not stale.
     */
    [[nodiscard]] bool active(
        Timestamp now,
        std::chrono::milliseconds timeout) const noexcept;

    /**
     * @brief Update the latest presence activity timestamp.
     *
     * @param now Activity timestamp.
     * @return Current presence record.
     *
     * @throws vix::realtime::Error
     *         When the presence already left or time moves backwards.
     */
    Presence &touch(
        Timestamp now = SystemClock::now());

    /**
     * @brief Mark the session as present after join or reconnection.
     *
     * The operation clears detachment information and optionally updates the
     * reporting node and active transport connection.
     *
     * @param connectionId Active connection identifier, or an empty string.
     * @param nodeId Optional reporting node. An absent value preserves the
     *               existing node.
     * @param now Presence update timestamp.
     * @return Current presence record.
     *
     * @throws vix::realtime::Error
     *         When the presence already left or time moves backwards.
     */
    Presence &mark_present(
        ConnectionId connectionId = {},
        std::optional<NodeId> nodeId = std::nullopt,
        Timestamp now = SystemClock::now());

    /**
     * @brief Mark the logical session as temporarily detached.
     *
     * The active connection identifier is removed. Calling this method on an
     * already detached presence is safe and updates `last_seen_at()`.
     *
     * @param now Detachment timestamp.
     * @return Current presence record.
     *
     * @throws vix::realtime::Error
     *         When the presence already left or time moves backwards.
     */
    Presence &mark_detached(
        Timestamp now = SystemClock::now());

    /**
     * @brief Mark the session as permanently absent from the room.
     *
     * Calling this method on an already left presence is harmless.
     *
     * @param now Leave timestamp.
     * @return Current presence record.
     *
     * @throws vix::realtime::Error
     *         When time moves backwards.
     */
    Presence &mark_left(
        Timestamp now = SystemClock::now());

    /**
     * @brief Replace the node reporting this presence.
     *
     * @param nodeId Non-empty runtime node identifier.
     * @return Current presence record.
     *
     * @throws vix::realtime::Error
     *         When the node identifier is empty or presence already left.
     */
    Presence &set_node_id(NodeId nodeId);

    /**
     * @brief Remove the reporting node identifier.
     *
     * @return Current presence record.
     */
    Presence &clear_node_id() noexcept;

    /**
     * @brief Replace the active transport connection identifier.
     *
     * Setting a connection automatically marks the presence as present.
     *
     * @param connectionId Non-empty connection identifier.
     * @param now Presence update timestamp.
     * @return Current presence record.
     *
     * @throws vix::realtime::Error
     *         When the connection identifier is empty, presence already left,
     *         or time moves backwards.
     */
    Presence &set_connection_id(
        ConnectionId connectionId,
        Timestamp now = SystemClock::now());

    /**
     * @brief Remove the transport connection and mark presence detached.
     *
     * @param now Detachment timestamp.
     * @return Current presence record.
     */
    Presence &clear_connection_id(
        Timestamp now = SystemClock::now());

    /**
     * @brief Return non-authoritative application presence metadata.
     *
     * @return Constant reference to presence metadata.
     */
    [[nodiscard]] const JsonObject &
    metadata() const noexcept;

    /**
     * @brief Replace non-authoritative presence metadata.
     *
     * @param value New presence metadata.
     * @return Current presence record.
     */
    Presence &set_metadata(JsonObject value);

    /**
     * @brief Return whether the presence record is internally consistent.
     *
     * @return True when identifiers, lifecycle state, and timestamps are valid.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /**
     * @brief Validate the logical room presence.
     *
     * @throws vix::realtime::Error
     *         When identifiers, timestamps, or lifecycle fields are invalid.
     */
    void validate() const;

  private:
    /** @brief Room containing the logical session. */
    RoomId roomId_{};

    /** @brief Logical session represented by the presence. */
    SessionId sessionId_{};

    /** @brief Application-defined authenticated identity. */
    Identity identity_{};

    /** @brief Optional runtime node reporting the presence. */
    std::optional<NodeId> nodeId_{};

    /** @brief Active transport connection identifier. */
    ConnectionId connectionId_{};

    /** @brief Current presence lifecycle status. */
    PresenceStatus status_{PresenceStatus::Left};

    /** @brief Time at which the session joined the room. */
    Timestamp joinedAt_{};

    /** @brief Latest observed presence activity. */
    Timestamp lastSeenAt_{};

    /** @brief Latest transport detachment timestamp. */
    std::optional<Timestamp> detachedAt_{};

    /** @brief Permanent room leave timestamp. */
    std::optional<Timestamp> leftAt_{};

    /** @brief Non-authoritative application presence metadata. */
    JsonObject metadata_{};
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_PRESENCE_HPP
