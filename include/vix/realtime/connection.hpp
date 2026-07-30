/**
 *
 * @file connection.hpp
 * @author Gaspard Kirira
 * @brief Transport-independent active connection interface for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_CONNECTION_HPP
#define VIX_REALTIME_CONNECTION_HPP

#include <memory>
#include <string_view>

#include <vix/realtime/api.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Active transport connection attached to a logical session.
   *
   * A connection represents one currently active client transport, such as a
   * WebSocket connection. It is intentionally independent from the logical
   * `Session`, which may survive transport disconnections and later resume
   * through another connection.
   *
   * Implementations are responsible for:
   *
   * - serializing or forwarding protocol envelopes;
   * - preserving message order for one connection;
   * - reporting whether the transport remains open;
   * - closing the underlying transport.
   */
  class VIX_REALTIME_API Connection
  {
  public:
    /**
     * @brief Destroy the connection.
     */
    virtual ~Connection() = default;

    /**
     * @brief Return the stable connection identifier.
     *
     * The returned reference must remain valid for the lifetime of the
     * connection.
     *
     * @return Connection identifier.
     */
    [[nodiscard]] virtual const ConnectionId &
    id() const noexcept = 0;

    /**
     * @brief Return whether the underlying transport is open.
     *
     * @return True when the connection may still send messages.
     */
    [[nodiscard]] virtual bool is_open() const noexcept = 0;

    /**
     * @brief Send one protocol envelope to the connected client.
     *
     * Implementations must preserve the order in which this method is called
     * for a single connection.
     *
     * @param envelope Protocol envelope to send.
     *
     * @throws vix::realtime::Error
     *         With `TransportFailure` when the message cannot be delivered.
     */
    virtual void send(
        const protocol::Envelope &envelope) = 0;

    /**
     * @brief Close the underlying client transport.
     *
     * Calling this method on an already closed connection should be harmless.
     *
     * @param code Reason represented by a Realtime error code.
     * @param reason Optional human-readable close reason.
     */
    virtual void close(
        ErrorCode code = ErrorCode::Cancelled,
        std::string_view reason = {}) = 0;

    /**
     * @brief Return transport-defined connection metadata.
     *
     * Metadata may include remote address, transport name, user agent, tracing
     * information, or adapter-specific values. It must not contain
     * authoritative room state.
     *
     * @return Connection metadata.
     */
    [[nodiscard]] virtual JsonObject metadata() const
    {
      return {};
    }
  };

  /**
   * @brief Shared ownership pointer for an active connection.
   */
  using ConnectionPtr = std::shared_ptr<Connection>;

  /**
   * @brief Weak ownership pointer for an active connection.
   */
  using WeakConnectionPtr = std::weak_ptr<Connection>;

} // namespace vix::realtime

#endif // VIX_REALTIME_CONNECTION_HPP
