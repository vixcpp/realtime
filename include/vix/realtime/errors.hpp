/**
 *
 * @file errors.hpp
 * @author Gaspard Kirira
 * @brief Error types and deterministic error codes for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ERRORS_HPP
#define VIX_REALTIME_ERRORS_HPP

#include <stdexcept>
#include <string>
#include <string_view>

#include <vix/realtime/api.hpp>

namespace vix::realtime
{
  /**
   * @brief Canonical error codes produced by the Realtime module.
   *
   * These codes are independent of the transport and may be serialized
   * by WebSocket, HTTP, tests, or another adapter.
   */
  enum class ErrorCode
  {
    /** @brief No error occurred. */
    None = 0,

    /** @brief The supplied configuration is invalid. */
    InvalidConfiguration,

    /** @brief A required dependency was not provided. */
    MissingDependency,

    /** @brief The requested room does not exist. */
    RoomNotFound,

    /** @brief The requested room already exists. */
    RoomAlreadyExists,

    /** @brief The room cannot accept more sessions. */
    RoomFull,

    /** @brief The room reached the configured runtime limit. */
    RoomLimitReached,

    /** @brief The room is opening or restoring its state. */
    RoomNotReady,

    /** @brief The room is closing or already closed. */
    RoomClosed,

    /** @brief The room command queue is full. */
    CommandQueueFull,

    /** @brief The command is malformed or unsupported. */
    InvalidCommand,

    /** @brief The command was rejected by the room handler. */
    CommandRejected,

    /** @brief The command exceeded its execution timeout. */
    CommandTimeout,

    /** @brief The caller is not authorized to perform the operation. */
    Unauthorized,

    /** @brief The logical realtime session does not exist. */
    SessionNotFound,

    /** @brief The logical realtime session has expired. */
    SessionExpired,

    /** @brief The supplied resume token is invalid. */
    InvalidResumeToken,

    /** @brief The connection is not attached to a logical session. */
    ConnectionNotAttached,

    /** @brief The requested room membership does not exist. */
    MembershipNotFound,

    /** @brief The session has already joined the room. */
    AlreadyJoined,

    /** @brief The protocol message is malformed. */
    InvalidProtocolMessage,

    /** @brief The protocol version is unsupported. */
    UnsupportedProtocolVersion,

    /** @brief The incoming payload exceeds the configured size limit. */
    PayloadTooLarge,

    /** @brief An event could not be persisted. */
    EventStoreFailure,

    /** @brief A snapshot could not be loaded or persisted. */
    SnapshotStoreFailure,

    /** @brief Stored room data is invalid or inconsistent. */
    CorruptedState,

    /** @brief An event could not be applied to the room state. */
    EventApplyFailure,

    /** @brief The requested replay cursor is invalid or unavailable. */
    ReplayUnavailable,

    /** @brief The replay exceeds configured limits. */
    ReplayLimitExceeded,

    /** @brief The transport failed while delivering a message. */
    TransportFailure,

    /** @brief The operation was cancelled. */
    Cancelled,

    /** @brief The operation exceeded its allowed duration. */
    Timeout,

    /** @brief An internal invariant was violated. */
    InternalError,

    /** @brief The session already has an active connection. */
    SessionAlreadyConnected,

    /** @brief The session has never been detached for resumption. */
    SessionNotDetached
  };

  /**
   * @brief Return the stable textual representation of an error code.
   *
   * The returned value is suitable for logs and protocol error responses.
   *
   * @param code Error code to convert.
   * @return Stable lowercase error identifier.
   */
  [[nodiscard]] VIX_REALTIME_API std::string_view
  to_string(ErrorCode code) noexcept;

  /**
   * @brief Exception carrying a deterministic Realtime error code.
   */
  class VIX_REALTIME_API Error : public std::runtime_error
  {
  public:
    /**
     * @brief Construct a Realtime error.
     *
     * @param code Deterministic error code.
     * @param message Human-readable diagnostic message.
     */
    Error(ErrorCode code, std::string message);

    /**
     * @brief Return the deterministic error code.
     *
     * @return Error code associated with this exception.
     */
    [[nodiscard]] ErrorCode code() const noexcept;

  private:
    /** @brief Deterministic error code associated with this exception. */
    ErrorCode code_{ErrorCode::InternalError};
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_ERRORS_HPP
