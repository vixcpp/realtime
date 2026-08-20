/**
 *
 * @file session_resume.hpp
 * @author Gaspard Kirira
 * @brief Logical session resumption service for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_SESSION_RESUME_HPP
#define VIX_REALTIME_SESSION_RESUME_HPP

#include <chrono>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/room_manager.hpp>
#include <vix/realtime/session.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace internal
  {
    class TokenGenerator;
  }

  /**
   * @brief Result of successfully resuming one logical session.
   */
  struct SessionResumeResult
  {
    /** @brief Resumed logical session. */
    SessionPtr session{};

    /** @brief Connection replaced during attachment, when present. */
    ConnectionPtr replacedConnection{};

    /** @brief Token that must be used for the next resumption. */
    ResumeToken resumeToken{};

    /** @brief Whether the previous token was rotated. */
    bool tokenRotated{false};

    /**
     * @brief Return whether a logical session was resumed.
     *
     * @return True when the result contains a session.
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
      return session != nullptr;
    }
  };

  /**
   * @brief Issues, validates, rotates, and revokes session resume tokens.
   *
   * Resume tokens are opaque one-session credentials. A successful resumption
   * verifies:
   *
   * - session resumption is enabled;
   * - the logical session still exists;
   * - the supplied token matches the current session token;
   * - the session is detached;
   * - the configured resume window has not elapsed;
   * - the replacement connection is open and valid.
   *
   * Tokens are rotated after successful resumption by default, preventing
   * repeated use of an already accepted credential.
   */
  class VIX_REALTIME_API SessionResume
  {
  public:
    /**
     * @brief Construct a session resumption service.
     *
     * @param manager Room manager containing logical sessions.
     *
     * @throws vix::realtime::Error
     *         When the room manager is null.
     */
    explicit SessionResume(
        RoomManagerPtr manager);

    SessionResume(
        RoomManagerPtr manager,
        std::chrono::milliseconds resumeWindow);

    /**
     * @brief Issue and store a new token for one logical session.
     *
     * Any previous token is immediately invalidated.
     *
     * @param sessionId Logical session identifier.
     * @return Newly issued resume token.
     *
     * @throws vix::realtime::Error
     *         When resumption is disabled or the session is unavailable.
     */
    [[nodiscard]] ResumeToken issue(
        const SessionId &sessionId);

    [[nodiscard]] ResumeToken issue(
        Session &session);

    /**
     * @brief Rotate the current token of one logical session.
     *
     * This operation has the same behavior as `issue()`.
     *
     * @param sessionId Logical session identifier.
     * @return Newly issued resume token.
     */
    [[nodiscard]] ResumeToken rotate(
        const SessionId &sessionId);

    [[nodiscard]] ResumeToken rotate(
        Session &session);

    /**
     * @brief Revoke the current token of one logical session.
     *
     * Revocation does not close the session or its active connection.
     *
     * @param sessionId Logical session identifier.
     * @return True when a non-empty token was revoked.
     *
     * @throws vix::realtime::Error
     *         When the session is not found.
     */
    bool revoke(
        const SessionId &sessionId);

    bool revoke(
        Session &session);

    /**
     * @brief Return whether a supplied token matches the session token.
     *
     * This method validates only token ownership. It does not require the
     * session to be detached or inside the resume window.
     *
     * @param sessionId Logical session identifier.
     * @param token Presented resume token.
     * @return True when the token matches.
     */
    [[nodiscard]] bool matches(
        const SessionId &sessionId,
        std::string_view token) const noexcept;

    [[nodiscard]] bool matches(
        const Session &session,
        std::string_view token) const noexcept;

    /**
     * @brief Return whether a session may currently be resumed.
     *
     * The token must match and the session must be detached inside the
     * configured resume window.
     *
     * @param sessionId Logical session identifier.
     * @param token Presented resume token.
     * @param now Current timestamp.
     * @return True when resumption may proceed.
     */
    [[nodiscard]] bool can_resume(
        const SessionId &sessionId,
        std::string_view token,
        Timestamp now = SystemClock::now()) const noexcept;

    [[nodiscard]] bool can_resume(
        const Session &session,
        std::string_view token,
        Timestamp now = SystemClock::now()) const noexcept;

    /**
     * @brief Resume a detached logical session on a new connection.
     *
     * The supplied token is verified before the connection is attached. When
     * `rotateToken` is true, a new token is generated and stored after the
     * connection attachment succeeds.
     *
     * @param sessionId Logical session identifier.
     * @param token Presented resume token.
     * @param connection Replacement transport connection.
     * @param now Resumption timestamp.
     * @param rotateToken Whether to rotate the token after success.
     * @return Successful session resumption result.
     *
     * @throws vix::realtime::Error
     *         With `SessionNotFound` for an unknown session,
     *         `InvalidResumeToken` for an invalid credential,
     *         `SessionAlreadyConnected` or `SessionNotDetached` for an
     *         ineligible lifecycle state, `SessionExpired` for a closed
     *         session or elapsed resume window, `CorruptedState` for a resume
     *         timestamp before detachment, or `ConnectionNotAttached` for an
     *         invalid replacement connection.
     */
    [[nodiscard]] SessionResumeResult resume(
        const SessionId &sessionId,
        std::string_view token,
        ConnectionPtr connection,
        Timestamp now = SystemClock::now(),
        bool rotateToken = true);

    [[nodiscard]] SessionResumeResult resume(
        const SessionPtr &session,
        ConnectionPtr connection,
        std::string_view token,
        Timestamp now = SystemClock::now(),
        bool rotateToken = true);

    [[nodiscard]] SessionResumeResult resume(
        Session &session,
        ConnectionPtr connection,
        std::string_view token,
        Timestamp now = SystemClock::now(),
        bool rotateToken = true);

    /**
     * @brief Return the configured resume window in milliseconds.
     *
     * @return Maximum detached session duration.
     */
    [[nodiscard]] std::chrono::milliseconds
    resume_window() const noexcept;

    /**
     * @brief Return the room manager used by the service.
     *
     * @return Shared room manager.
     */
    [[nodiscard]] const RoomManagerPtr &
    manager() const noexcept;

  private:
    /**
     * @brief Ensure session resumption is enabled.
     *
     * @throws vix::realtime::Error
     *         When the runtime disabled session resumption.
     */
    void require_enabled() const;

    /**
     * @brief Compare two token values without content-dependent early exit.
     *
     * @param left First token.
     * @param right Second token.
     * @return True when both token values are identical.
     */
    [[nodiscard]] static bool secure_equal(
        std::string_view left,
        std::string_view right) noexcept;

    /**
     * @brief Validate a presented token against one session.
     *
     * The caller must hold `mutex_`.
     *
     * @param session Logical session.
     * @param token Presented token.
     *
     * @throws vix::realtime::Error
     *         When the token is missing, malformed, or does not match.
     */
    void validate_token_locked(
        const SessionPtr &session,
        std::string_view token) const;

    [[nodiscard]] SessionResumeResult resume_session_locked(
        const SessionPtr &session,
        ConnectionPtr connection,
        std::string_view token,
        Timestamp now,
        bool rotateToken);

    [[nodiscard]] std::vector<std::pair<RoomId, EventId>>
    replay_rooms_locked(
        Session &session,
        const ConnectionPtr &connection) const;

    /** @brief Manager containing logical sessions and connections. */
    RoomManagerPtr manager_{};

    /** @brief Opaque random resume token generator. */
    std::shared_ptr<internal::TokenGenerator> tokenGenerator_{};

    /** @brief Whether a constructor supplied an explicit resume window. */
    bool hasCustomResumeWindow_{false};

    /** @brief Explicit resume window used by compatibility constructors. */
    std::chrono::milliseconds customResumeWindow_{};

    /** @brief Serializes token validation, rotation, and attachment. */
    mutable std::mutex mutex_{};
  };

  /**
   * @brief Shared ownership pointer for a session resumption service.
   */
  using SessionResumePtr =
      std::shared_ptr<SessionResume>;

} // namespace vix::realtime

#endif // VIX_REALTIME_SESSION_RESUME_HPP
