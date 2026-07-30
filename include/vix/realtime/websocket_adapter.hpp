/**
 *
 * @file websocket_adapter.hpp
 * @author Gaspard Kirira
 * @brief Vix WebSocket transport adapter for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_WEBSOCKET_ADAPTER_HPP
#define VIX_REALTIME_WEBSOCKET_ADAPTER_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/transport.hpp>

namespace vix::websocket
{
  class Server;
}

namespace vix::realtime
{
  /**
   * @brief Configuration for the Vix WebSocket Realtime adapter.
   */
  struct WebSocketAdapterOptions
  {
    /**
     * @brief Maximum accepted raw WebSocket message size.
     *
     * Zero disables the adapter-level message size limit.
     */
    std::size_t maxMessageSize{64U * 1024U};

    /**
     * @brief Close a connection after invalid protocol input.
     */
    bool closeOnProtocolError{true};

    /**
     * @brief Prefix used when generating transport connection identifiers.
     */
    std::string connectionIdPrefix{"ws"};

    /**
     * @brief Validate adapter options.
     *
     * @throws vix::realtime::Error
     *         When the connection identifier prefix is invalid.
     */
    void validate() const;
  };

  /**
   * @brief Adapts `vix::websocket::Server` to the Realtime transport contract.
   *
   * The adapter registers raw WebSocket callbacks:
   *
   * - `on_open`;
   * - `on_message`;
   * - `on_close`;
   * - `on_error`.
   *
   * Raw WebSocket messages are parsed with `protocol::parse()`. Outgoing
   * Realtime envelopes are serialized and forwarded through
   * `vix::websocket::Session::send_text()`.
   *
   * The adapter does not start or stop the WebSocket server, its executor, or
   * an attached HTTP runtime.
   *
   * Installing the adapter replaces the corresponding callbacks currently
   * configured on the WebSocket server.
   */
  class VIX_REALTIME_API WebSocketAdapter final
      : public Transport
  {
  public:
    /**
     * @brief Construct a WebSocket transport adapter.
     *
     * @param websocketServer Existing Vix WebSocket server.
     * @param options Adapter configuration.
     *
     * @throws vix::realtime::Error
     *         When adapter options are invalid.
     */
    explicit WebSocketAdapter(
        vix::websocket::Server &websocketServer,
        WebSocketAdapterOptions options = {});

    /**
     * @brief Destroy and detach the adapter.
     */
    ~WebSocketAdapter() override;

    WebSocketAdapter(const WebSocketAdapter &) = delete;
    WebSocketAdapter &operator=(const WebSocketAdapter &) = delete;
    WebSocketAdapter(WebSocketAdapter &&) = delete;
    WebSocketAdapter &operator=(WebSocketAdapter &&) = delete;

    /**
     * @brief Replace every transport callback.
     *
     * @param handlers New callback collection.
     */
    void set_handlers(
        TransportHandlers handlers) override;

    /**
     * @brief Return a copy of the configured transport callbacks.
     *
     * @return Current callback collection.
     */
    [[nodiscard]] TransportHandlers
    handlers() const override;

    /**
     * @brief Install callbacks on the underlying WebSocket server.
     *
     * This operation does not start the WebSocket server.
     *
     * @return True when the adapter transitioned to attached.
     */
    bool attach() override;

    /**
     * @brief Stop forwarding WebSocket activity to Realtime callbacks.
     *
     * Existing WebSocket sessions are not closed. Their Realtime connection
     * wrappers are marked closed and removed from the adapter.
     *
     * @return True when the adapter transitioned to detached.
     */
    bool detach() override;

    /**
     * @brief Return whether the adapter is attached.
     *
     * @return True when WebSocket callbacks are active.
     */
    [[nodiscard]] bool attached() const override;

    /**
     * @brief Return the number of tracked WebSocket connections.
     *
     * @return Tracked connection count.
     */
    [[nodiscard]] std::size_t
    connection_count() const override;

    /**
     * @brief Find one active connection by generated identifier.
     *
     * @param connectionId Connection identifier.
     * @return Connection wrapper, or null when absent.
     */
    [[nodiscard]] ConnectionPtr find_connection(
        const ConnectionId &connectionId) const;

    /**
     * @brief Return all tracked connections.
     *
     * Results are sorted by connection identifier.
     *
     * @return Active connection wrappers.
     */
    [[nodiscard]] std::vector<ConnectionPtr>
    connections() const;

    /**
     * @brief Return the underlying WebSocket server.
     *
     * @return WebSocket server reference.
     */
    [[nodiscard]] vix::websocket::Server &
    websocket_server() noexcept;

    /**
     * @brief Return the adapter configuration.
     *
     * @return Constant adapter options reference.
     */
    [[nodiscard]] const WebSocketAdapterOptions &
    options() const noexcept;

  private:
    /**
     * @brief Shared callback state captured safely by WebSocket handlers.
     */
    struct State;

    /** @brief Existing WebSocket server adapted by this instance. */
    vix::websocket::Server &websocketServer_;

    /** @brief Shared state retained by installed callbacks. */
    std::shared_ptr<State> state_;
  };

  /**
   * @brief Shared ownership pointer for a WebSocket adapter.
   */
  using WebSocketAdapterPtr =
      std::shared_ptr<WebSocketAdapter>;

} // namespace vix::realtime

#endif // VIX_REALTIME_WEBSOCKET_ADAPTER_HPP
