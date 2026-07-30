/**
 *
 * @file session.hpp
 * @author Gaspard Kirira
 * @brief Logical resumable client session for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_SESSION_HPP
#define VIX_REALTIME_SESSION_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Lifecycle state of a logical Realtime session.
   */
  enum class SessionStatus : std::uint8_t
  {
    /** @brief The session currently owns an open connection. */
    Connected = 0,

    /** @brief The session has no active connection but may be resumed. */
    Detached,

    /** @brief The session was permanently closed. */
    Closed
  };

  /**
   * @brief Return the stable textual representation of a session status.
   *
   * @param status Session status.
   * @return Stable lowercase identifier.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(SessionStatus status) noexcept
  {
    switch (status)
    {
    case SessionStatus::Connected:
      return "connected";

    case SessionStatus::Detached:
      return "detached";

    case SessionStatus::Closed:
      return "closed";
    }

    return "closed";
  }

  /**
   * @brief Logical client session that may survive transport disconnections.
   *
   * A session owns client identity, room memberships, resume information, and
   * at most one active transport connection.
   *
   * A connection may be replaced during reconnection without replacing the
   * logical session or losing its room memberships.
   */
  class VIX_REALTIME_API Session
  {
  public:
    /**
     * @brief Construct a logical session.
     *
     * @param sessionId Stable logical session identifier.
     * @param identity Application-defined authenticated identity.
     * @param resumeToken Token used to resume the detached session.
     * @param createdAt Session creation timestamp.
     * @param metadata Application-defined session metadata.
     *
     * @throws vix::realtime::Error
     *         When the session identifier is empty.
     */
    explicit Session(
        SessionId sessionId,
        Identity identity = {},
        ResumeToken resumeToken = {},
        Timestamp createdAt = SystemClock::now(),
        JsonObject metadata = {});

    /**
     * @brief Destroy the logical session.
     */
    ~Session() = default;

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    Session(Session &&) = delete;
    Session &operator=(Session &&) = delete;

    /**
     * @brief Return the logical session identifier.
     *
     * @return Session identifier.
     */
    [[nodiscard]] const SessionId &
    id() const noexcept;

    /**
     * @brief Return the application-defined client identity.
     *
     * The identity is immutable for the lifetime of the session.
     *
     * @return Client identity, or an empty string when anonymous.
     */
    [[nodiscard]] const Identity &
    identity() const noexcept;

    /**
     * @brief Return the session creation timestamp.
     *
     * @return Session creation timestamp.
     */
    [[nodiscard]] Timestamp created_at() const noexcept;

    /**
     * @brief Return the current session lifecycle status.
     *
     * @return Connected, detached, or closed status.
     */
    [[nodiscard]] SessionStatus status() const;

    /**
     * @brief Return whether the session has an open active connection.
     *
     * @return True when an open connection is attached.
     */
    [[nodiscard]] bool connected() const;

    /**
     * @brief Return whether the session is permanently closed.
     *
     * @return True when the session cannot be attached or resumed.
     */
    [[nodiscard]] bool closed() const;

    /**
     * @brief Return the currently attached connection.
     *
     * @return Active connection, or null when detached.
     */
    [[nodiscard]] ConnectionPtr connection() const;

    /**
     * @brief Return the current connection identifier.
     *
     * @return Connection ID, or an empty string when detached.
     */
    [[nodiscard]] ConnectionId connection_id() const;

    /**
     * @brief Attach or replace the active transport connection.
     *
     * Replacing a connection is useful during session resumption. The previous
     * connection is returned so the caller may close it outside the session
     * lock.
     *
     * @param connection Open connection to attach.
     * @param now Attachment timestamp.
     * @return Previously attached connection, or null.
     *
     * @throws vix::realtime::Error
     *         When the session is closed or the connection is invalid.
     */
    [[nodiscard]] ConnectionPtr attach(
        ConnectionPtr connection,
        Timestamp now = SystemClock::now());

    /**
     * @brief Detach a specific active connection.
     *
     * The connection is detached only when its identifier matches the
     * currently attached connection. This prevents an old transport close
     * callback from detaching a newer resumed connection.
     *
     * @param connectionId Connection expected to be currently attached.
     * @param now Detachment timestamp.
     * @return Detached connection, or null when the identifier did not match.
     *
     * @throws vix::realtime::Error
     *         When the supplied connection identifier is empty.
     */
    [[nodiscard]] ConnectionPtr detach(
        const ConnectionId &connectionId,
        Timestamp now = SystemClock::now());

    /**
     * @brief Detach the currently active connection.
     *
     * @param now Detachment timestamp.
     * @return Detached connection, or null when already detached.
     */
    [[nodiscard]] ConnectionPtr detach(
        Timestamp now = SystemClock::now());

    /**
     * @brief Permanently close the logical session.
     *
     * Room memberships remain available so the room manager can perform
     * deterministic membership cleanup afterward.
     *
     * @param now Closure timestamp.
     * @return Previously attached connection, or null.
     */
    [[nodiscard]] ConnectionPtr close(
        Timestamp now = SystemClock::now());

    /**
     * @brief Send one protocol envelope through the active connection.
     *
     * The connection is copied under the session lock and invoked afterward.
     *
     * @param envelope Envelope to deliver.
     *
     * @throws vix::realtime::Error
     *         When the session is closed, detached, or the connection is no
     *         longer open.
     */
    void send(const protocol::Envelope &envelope) const;

    /**
     * @brief Update the latest observed session activity timestamp.
     *
     * @param now Activity timestamp.
     */
    void touch(
        Timestamp now = SystemClock::now());

    /**
     * @brief Return the latest session activity timestamp.
     *
     * @return Latest activity timestamp.
     */
    [[nodiscard]] Timestamp last_seen_at() const;

    /**
     * @brief Return the latest detachment timestamp.
     *
     * @return Detachment timestamp, when detached at least once.
     */
    [[nodiscard]] std::optional<Timestamp>
    detached_at() const;

    /**
     * @brief Return the permanent closure timestamp.
     *
     * @return Closure timestamp, when the session is closed.
     */
    [[nodiscard]] std::optional<Timestamp>
    closed_at() const;

    /**
     * @brief Return the current session resume token.
     *
     * @return Resume token, or an empty string when resumption is disabled.
     */
    [[nodiscard]] ResumeToken resume_token() const;

    /**
     * @brief Replace the session resume token.
     *
     * @param value New resume token.
     *
     * @throws vix::realtime::Error
     *         When the session is already closed.
     */
    void set_resume_token(ResumeToken value);

    /**
     * @brief Remove the current resume token.
     */
    void clear_resume_token();

    /**
     * @brief Return whether the detached session may currently resume.
     *
     * A session may resume only when:
     *
     * - it is not closed;
     * - no open connection is attached;
     * - a resume token exists;
     * - it has a detachment timestamp;
     * - the resume window has not elapsed.
     *
     * @param now Current timestamp.
     * @param resumeWindow Maximum permitted detached duration.
     * @return True when resumption remains permitted.
     */
    [[nodiscard]] bool can_resume(
        Timestamp now,
        std::chrono::milliseconds resumeWindow) const;

    /**
     * @brief Add a room membership to the logical session.
     *
     * @param roomId Joined room identifier.
     * @return True when a new membership was inserted.
     *
     * @throws vix::realtime::Error
     *         When the session is closed or the room identifier is empty.
     */
    bool join_room(const RoomId &roomId);

    /**
     * @brief Remove a room membership from the logical session.
     *
     * @param roomId Room identifier to remove.
     * @return True when an existing membership was removed.
     *
     * @throws vix::realtime::Error
     *         When the room identifier is empty.
     */
    bool leave_room(const RoomId &roomId);

    /**
     * @brief Return whether the session belongs to a room.
     *
     * @param roomId Room identifier to inspect.
     * @return True when the room membership exists.
     */
    [[nodiscard]] bool has_room(
        const RoomId &roomId) const;

    /**
     * @brief Return all room memberships in deterministic order.
     *
     * @return Joined room identifiers.
     */
    [[nodiscard]] std::vector<RoomId> rooms() const;

    /**
     * @brief Return the number of joined rooms.
     *
     * @return Room membership count.
     */
    [[nodiscard]] std::size_t room_count() const;

    /**
     * @brief Remove all room memberships.
     *
     * @return Number of removed memberships.
     */
    std::size_t clear_rooms();

    /**
     * @brief Return application-defined session metadata.
     *
     * @return Copy of the current metadata.
     */
    [[nodiscard]] JsonObject metadata() const;

    /**
     * @brief Replace application-defined session metadata.
     *
     * @param value New session metadata.
     */
    void set_metadata(JsonObject value);

    /**
     * @brief Return whether the session state is internally consistent.
     *
     * @return True when the session is valid.
     */
    [[nodiscard]] bool is_valid() const;

    /**
     * @brief Validate the logical session.
     *
     * @throws vix::realtime::Error
     *         When the session contains inconsistent lifecycle information.
     */
    void validate() const;

  private:
    /** @brief Stable logical session identifier. */
    SessionId sessionId_{};

    /** @brief Immutable application-defined client identity. */
    Identity identity_{};

    /** @brief Session creation timestamp. */
    Timestamp createdAt_{SystemClock::now()};

    /** @brief Protects mutable session state. */
    mutable std::mutex mutex_{};

    /** @brief Current active transport connection. */
    ConnectionPtr connection_{};

    /** @brief Token used to resume a detached session. */
    ResumeToken resumeToken_{};

    /** @brief Latest observed session activity. */
    Timestamp lastSeenAt_{SystemClock::now()};

    /** @brief Latest transport detachment time. */
    std::optional<Timestamp> detachedAt_{};

    /** @brief Permanent session closure time. */
    std::optional<Timestamp> closedAt_{};

    /** @brief Joined rooms in deterministic identifier order. */
    std::set<RoomId> rooms_{};

    /** @brief Non-authoritative application session metadata. */
    JsonObject metadata_{};
  };

  /**
   * @brief Shared ownership pointer for a logical session.
   */
  using SessionPtr = std::shared_ptr<Session>;

  /**
   * @brief Weak ownership pointer for a logical session.
   */
  using WeakSessionPtr = std::weak_ptr<Session>;

} // namespace vix::realtime

#endif // VIX_REALTIME_SESSION_HPP
