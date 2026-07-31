/**
 *
 * @file transport.hpp
 * @author Gaspard Kirira
 * @brief Transport adapter contract for Vix Realtime protocol envelopes.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_TRANSPORT_HPP
#define VIX_REALTIME_TRANSPORT_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <vix/realtime/api.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/protocol.hpp>

namespace vix::realtime
{
  /**
   * @brief Callback invoked when a transport connection becomes available.
   */
  using TransportOpenHandler =
      std::function<void(ConnectionPtr)>;

  /**
   * @brief Callback invoked when a complete protocol envelope is received.
   */
  using TransportEnvelopeHandler =
      std::function<void(
          ConnectionPtr,
          const protocol::Envelope &)>;

  /**
   * @brief Callback invoked when a transport connection closes.
   */
  using TransportCloseHandler =
      std::function<void(ConnectionPtr)>;

  /**
   * @brief Callback invoked when transport or protocol processing fails.
   *
   * The connection may be null when the failure cannot be associated with one
   * active client.
   */
  using TransportErrorHandler =
      std::function<void(
          ConnectionPtr,
          ErrorCode,
          std::string_view)>;

  /**
   * @brief Collection of callbacks installed on a transport adapter.
   */
  struct TransportHandlers
  {
    /** @brief Active connection notification. */
    TransportOpenHandler onOpen{};

    /** @brief Parsed protocol envelope notification. */
    TransportEnvelopeHandler onEnvelope{};

    /** @brief Closed connection notification. */
    TransportCloseHandler onClose{};

    /** @brief Transport or protocol failure notification. */
    TransportErrorHandler onError{};

    /**
     * @brief Return whether any callback is configured.
     *
     * @return True when at least one callback exists.
     */
    [[nodiscard]] bool empty() const noexcept
    {
      return !onOpen &&
             !onEnvelope &&
             !onClose &&
             !onError;
    }
  };

  /**
   * @brief Transport adapter for Realtime protocol envelopes.
   *
   * A transport converts transport-specific connections and messages into:
   *
   * - `Connection` objects;
   * - parsed `protocol::Envelope` values;
   * - lifecycle and error callbacks.
   *
   * The transport does not own the Realtime server and does not interpret
   * application commands. It only bridges transport-specific I/O with the
   * transport-independent Realtime protocol.
   */
  class VIX_REALTIME_API Transport
  {
  public:
    /**
     * @brief Destroy the transport adapter.
     */
    virtual ~Transport() = default;

    /**
     * @brief Replace every transport callback.
     *
     * @param handlers New callback collection.
     */
    virtual void set_handlers(
        TransportHandlers handlers) = 0;

    /**
     * @brief Return a copy of the configured callbacks.
     *
     * @return Current callback collection.
     */
    [[nodiscard]] virtual TransportHandlers
    handlers() const = 0;

    /**
     * @brief Attach the adapter to its underlying transport.
     *
     * This operation installs transport-specific callbacks. It does not
     * necessarily start a listener or event loop.
     *
     * @return True when the adapter transitioned to attached.
     */
    virtual bool attach() = 0;

    /**
     * @brief Detach the adapter from active Realtime processing.
     *
     * Implementations must stop forwarding new transport events after this
     * operation. They are not required to stop the underlying listener.
     *
     * @return True when the adapter transitioned to detached.
     */
    virtual bool detach() = 0;

    /**
     * @brief Return whether the adapter is attached.
     *
     * @return True when transport events are being forwarded.
     */
    [[nodiscard]] virtual bool attached() const = 0;

    /**
     * @brief Return the number of currently tracked connections.
     *
     * @return Active transport connection count.
     */
    [[nodiscard]] virtual std::size_t
    connection_count() const = 0;
  };

  /**
   * @brief Shared ownership pointer for a transport adapter.
   */
  using TransportPtr = std::shared_ptr<Transport>;

} // namespace vix::realtime

#endif // VIX_REALTIME_TRANSPORT_HPP
