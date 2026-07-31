/**
 *
 * @file websocket_adapter.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix WebSocket adapter for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/websocket_adapter.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <exception>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/websocket/server.hpp>
#include <vix/websocket/session.hpp>

namespace vix::realtime
{
  namespace
  {
    /**
     * @brief Active Realtime connection backed by a WebSocket session.
     */
    class WebSocketConnection final
        : public Connection
    {
    public:
      /**
       * @brief Construct a WebSocket-backed Realtime connection.
       */
      WebSocketConnection(
          ConnectionId connectionId,
          std::shared_ptr<vix::websocket::Session> session)
          : connectionId_(std::move(connectionId)),
            session_(std::move(session)),
            open_(true)
      {
        if (connectionId_.empty())
        {
          throw Error{
              ErrorCode::InvalidConfiguration,
              "websocket connection requires an identifier"};
        }

        if (session_.expired())
        {
          throw Error{
              ErrorCode::ConnectionNotAttached,
              "websocket connection requires an active session"};
        }
      }

      /**
       * @brief Return the generated transport connection identifier.
       */
      [[nodiscard]] const ConnectionId &
      id() const noexcept override
      {
        return connectionId_;
      }

      /**
       * @brief Return whether the WebSocket wrapper remains open.
       */
      [[nodiscard]] bool is_open() const noexcept override
      {
        return open_.load(
                   std::memory_order_acquire) &&
               !session_.expired();
      }

      /**
       * @brief Serialize and send one Realtime protocol envelope.
       */
      void send(
          const protocol::Envelope &envelope) override
      {
        if (!open_.load(
                std::memory_order_acquire))
        {
          throw Error{
              ErrorCode::ConnectionNotAttached,
              "websocket connection is closed"};
        }

        envelope.validate();

        const std::string payload =
            protocol::serialize(envelope);

        std::shared_ptr<vix::websocket::Session> session =
            session_.lock();

        if (!session)
        {
          open_.store(
              false,
              std::memory_order_release);

          throw Error{
              ErrorCode::TransportFailure,
              "websocket session is no longer available"};
        }

        std::lock_guard<std::mutex> lock{sendMutex_};

        try
        {
          session->send_text(payload);
        }
        catch (const Error &)
        {
          throw;
        }
        catch (const std::exception &error)
        {
          throw Error{
              ErrorCode::TransportFailure,
              std::string{
                  "failed to send websocket message: "} +
                  error.what()};
        }
        catch (...)
        {
          throw Error{
              ErrorCode::TransportFailure,
              "failed to send websocket message"};
        }
      }

      /**
       * @brief Close the underlying WebSocket session.
       */
      void close(
          ErrorCode code,
          std::string_view reason) override
      {
        static_cast<void>(code);
        static_cast<void>(reason);

        const bool wasOpen =
            open_.exchange(
                false,
                std::memory_order_acq_rel);

        if (!wasOpen)
        {
          return;
        }

        std::shared_ptr<vix::websocket::Session> session =
            session_.lock();

        if (!session)
        {
          return;
        }

        try
        {
          session->close();
        }
        catch (const std::exception &error)
        {
          throw Error{
              ErrorCode::TransportFailure,
              std::string{
                  "failed to close websocket connection: "} +
                  error.what()};
        }
        catch (...)
        {
          throw Error{
              ErrorCode::TransportFailure,
              "failed to close websocket connection"};
        }
      }

      /**
       * @brief Return WebSocket transport metadata.
       */
      [[nodiscard]] JsonObject metadata() const override
      {
        JsonObject result;

        result.set_string(
            "transport",
            "websocket");

        result.set_string(
            "connection_id",
            connectionId_);

        return result;
      }

      /**
       * @brief Mark the wrapper closed without closing the WebSocket session.
       */
      void mark_closed() noexcept
      {
        open_.store(
            false,
            std::memory_order_release);
      }

    private:
      /** @brief Generated process-local connection identifier. */
      ConnectionId connectionId_{};

      /** @brief Underlying WebSocket session. */
      std::weak_ptr<vix::websocket::Session> session_{};

      /** @brief Serializes concurrent outgoing sends. */
      mutable std::mutex sendMutex_{};

      /** @brief Adapter-level connection state. */
      std::atomic<bool> open_{false};
    };

    /**
     * @brief Return whether a connection prefix character is permitted.
     */
    [[nodiscard]] bool valid_prefix_character(
        unsigned char value) noexcept
    {
      return std::isalnum(value) != 0 ||
             value == '_' ||
             value == '-' ||
             value == '.';
    }

  } // namespace

  struct AdapterState
  {
    /** @brief Protects callbacks and connection maps. */
    mutable std::mutex mutex{};

    /** @brief Adapter configuration. */
    WebSocketAdapterOptions options{};

    /** @brief Current transport callbacks. */
    TransportHandlers handlers{};

    /** @brief Whether WebSocket events are currently forwarded. */
    bool attached{false};

    /** @brief Next generated WebSocket connection number. */
    std::atomic<std::uint64_t> nextConnectionId{1};

    /** @brief Connections indexed by WebSocket session address. */
    std::unordered_map<
        vix::websocket::Session *,
        std::shared_ptr<WebSocketConnection>>
        bySession{};

    /** @brief Connections indexed by generated identifier. */
    std::unordered_map<
        ConnectionId,
        std::shared_ptr<WebSocketConnection>>
        byId{};
  };

  struct WebSocketAdapter::State : AdapterState
  {
  };

  namespace
  {
    /**
     * @brief Generate the next adapter connection identifier.
     */
    [[nodiscard]] ConnectionId make_connection_id(
        AdapterState &state)
    {
      const std::uint64_t value =
          state.nextConnectionId.fetch_add(
              1,
              std::memory_order_relaxed);

      return state.options.connectionIdPrefix +
             "-" +
             std::to_string(value);
    }

    /**
     * @brief Copy the current error callback.
     */
    [[nodiscard]] TransportErrorHandler error_handler(
        const std::shared_ptr<AdapterState> &state)
    {
      std::lock_guard<std::mutex> lock{
          state->mutex};

      if (!state->attached)
      {
        return {};
      }

      return state->handlers.onError;
    }

    /**
     * @brief Report one transport failure without propagating callback errors.
     */
    void report_error(
        const std::shared_ptr<AdapterState> &state,
        ConnectionPtr connection,
        ErrorCode code,
        std::string message) noexcept
    {
      TransportErrorHandler handler =
          error_handler(state);

      if (!handler)
      {
        return;
      }

      try
      {
        handler(
            std::move(connection),
            code,
            message);
      }
      catch (...)
      {
      }
    }

    /**
     * @brief Find the connection associated with a WebSocket session.
     */
    [[nodiscard]] std::shared_ptr<WebSocketConnection>
    find_session_connection(
        const std::shared_ptr<AdapterState> &state,
        vix::websocket::Session &session)
    {
      std::lock_guard<std::mutex> lock{
          state->mutex};

      if (!state->attached)
      {
        return nullptr;
      }

      const auto iterator =
          state->bySession.find(&session);

      if (iterator == state->bySession.end())
      {
        return nullptr;
      }

      return iterator->second;
    }

    /**
     * @brief Process one WebSocket open callback.
     */
    void handle_open(
        const std::shared_ptr<AdapterState> &state,
        vix::websocket::Session &session)
    {
      std::shared_ptr<WebSocketConnection> connection;
      TransportOpenHandler handler;

      try
      {
        std::shared_ptr<vix::websocket::Session> websocketSession =
            session.shared_from_this();

        {
          std::lock_guard<std::mutex> lock{
              state->mutex};

          if (!state->attached)
          {
            return;
          }

          const auto existing =
              state->bySession.find(&session);

          if (existing != state->bySession.end())
          {
            connection = existing->second;
          }
          else
          {
            connection =
                std::make_shared<WebSocketConnection>(
                    make_connection_id(*state),
                    std::move(websocketSession));

            state->bySession.emplace(
                &session,
                connection);

            state->byId.emplace(
                connection->id(),
                connection);
          }

          handler =
              state->handlers.onOpen;
        }

        if (handler)
        {
          handler(connection);
        }
      }
      catch (const Error &error)
      {
        report_error(
            state,
            connection,
            error.code(),
            error.what());
      }
      catch (const std::exception &error)
      {
        report_error(
            state,
            connection,
            ErrorCode::TransportFailure,
            error.what());
      }
      catch (...)
      {
        report_error(
            state,
            connection,
            ErrorCode::TransportFailure,
            "failed to initialize websocket connection");
      }
    }

    /**
     * @brief Process one raw WebSocket message.
     */
    void handle_message(
        const std::shared_ptr<AdapterState> &state,
        vix::websocket::Session &session,
        const std::string &message)
    {
      std::shared_ptr<WebSocketConnection> connection =
          find_session_connection(
              state,
              session);

      if (!connection)
      {
        report_error(
            state,
            nullptr,
            ErrorCode::ConnectionNotAttached,
            "websocket message arrived for an unknown connection");

        return;
      }

      WebSocketAdapterOptions options;
      TransportEnvelopeHandler handler;

      {
        std::lock_guard<std::mutex> lock{
            state->mutex};

        if (!state->attached)
        {
          return;
        }

        options = state->options;
        handler = state->handlers.onEnvelope;
      }

      ErrorCode failureCode =
          ErrorCode::None;

      std::string failureMessage;

      try
      {
        if (options.maxMessageSize != 0 &&
            message.size() >
                options.maxMessageSize)
        {
          throw Error{
              ErrorCode::PayloadTooLarge,
              "websocket message exceeds the configured size limit"};
        }

        protocol::Envelope envelope =
            protocol::parse(message);

        envelope.validate();

        if (handler)
        {
          handler(
              connection,
              envelope);
        }

        return;
      }
      catch (const Error &error)
      {
        failureCode = error.code();
        failureMessage = error.what();
      }
      catch (const std::exception &error)
      {
        failureCode =
            ErrorCode::InvalidProtocolMessage;

        failureMessage = error.what();
      }
      catch (...)
      {
        failureCode =
            ErrorCode::InvalidProtocolMessage;

        failureMessage =
            "failed to parse websocket protocol message";
      }

      report_error(
          state,
          connection,
          failureCode,
          failureMessage);

      if (options.closeOnProtocolError)
      {
        try
        {
          connection->close(
              failureCode,
              failureMessage);
        }
        catch (...)
        {
        }
      }
    }

    /**
     * @brief Process one WebSocket close callback.
     */
    void handle_close(
        const std::shared_ptr<AdapterState> &state,
        vix::websocket::Session &session)
    {
      std::shared_ptr<WebSocketConnection> connection;
      TransportCloseHandler handler;

      {
        std::lock_guard<std::mutex> lock{
            state->mutex};

        const auto iterator =
            state->bySession.find(&session);

        if (iterator == state->bySession.end())
        {
          return;
        }

        connection =
            iterator->second;

        state->byId.erase(
            connection->id());

        state->bySession.erase(
            iterator);

        connection->mark_closed();

        if (state->attached)
        {
          handler =
              state->handlers.onClose;
        }
      }

      if (handler)
      {
        try
        {
          handler(connection);
        }
        catch (...)
        {
        }
      }
    }

    /**
     * @brief Process one WebSocket transport error callback.
     */
    void handle_websocket_error(
        const std::shared_ptr<AdapterState> &state,
        vix::websocket::Session &session,
        const std::string &message)
    {
      ConnectionPtr connection =
          find_session_connection(
              state,
              session);

      report_error(
          state,
          std::move(connection),
          ErrorCode::TransportFailure,
          message);
    }

  } // namespace

  void WebSocketAdapterOptions::validate() const
  {
    if (connectionIdPrefix.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "websocket connection identifier prefix cannot be empty"};
    }

    if (connectionIdPrefix.size() > 32)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "websocket connection identifier prefix exceeds 32 characters"};
    }

    for (const unsigned char character :
         connectionIdPrefix)
    {
      if (!valid_prefix_character(character))
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "websocket connection identifier prefix contains invalid characters"};
      }
    }
  }

  WebSocketAdapter::WebSocketAdapter(
      vix::websocket::Server &websocketServer,
      WebSocketAdapterOptions options)
      : websocketServer_(websocketServer),
        state_(std::make_shared<State>())
  {
    options.validate();
    state_->options = std::move(options);
  }

  WebSocketAdapter::~WebSocketAdapter()
  {
    static_cast<void>(detach());
  }

  void WebSocketAdapter::set_handlers(
      TransportHandlers handlers)
  {
    std::lock_guard<std::mutex> lock{
        state_->mutex};

    state_->handlers =
        std::move(handlers);
  }

  TransportHandlers
  WebSocketAdapter::handlers() const
  {
    std::lock_guard<std::mutex> lock{
        state_->mutex};

    return state_->handlers;
  }

  bool WebSocketAdapter::attach()
  {
    {
      std::lock_guard<std::mutex> lock{
          state_->mutex};

      if (state_->attached)
      {
        return false;
      }

      state_->attached = true;
    }

    const std::weak_ptr<State> weakState =
        state_;

    websocketServer_.on_open(
        [weakState](
            vix::websocket::Session &session)
        {
          const std::shared_ptr<State> state =
              weakState.lock();

          if (!state)
          {
            return;
          }

          handle_open(
              state,
              session);
        });

    websocketServer_.on_message(
        [weakState](
            vix::websocket::Session &session,
            const std::string &message)
        {
          const std::shared_ptr<State> state =
              weakState.lock();

          if (!state)
          {
            return;
          }

          handle_message(
              state,
              session,
              message);
        });

    websocketServer_.on_close(
        [weakState](
            vix::websocket::Session &session)
        {
          const std::shared_ptr<State> state =
              weakState.lock();

          if (!state)
          {
            return;
          }

          handle_close(
              state,
              session);
        });

    websocketServer_.on_error(
        [weakState](
            vix::websocket::Session &session,
            const std::string &message)
        {
          const std::shared_ptr<State> state =
              weakState.lock();

          if (!state)
          {
            return;
          }

          handle_websocket_error(
              state,
              session,
              message);
        });

    return true;
  }

  bool WebSocketAdapter::detach()
  {
    std::vector<
        std::shared_ptr<WebSocketConnection>>
        connections;

    {
      std::lock_guard<std::mutex> lock{
          state_->mutex};

      if (!state_->attached)
      {
        return false;
      }

      state_->attached = false;

      connections.reserve(
          state_->byId.size());

      for (const auto &[connectionId, connection] :
           state_->byId)
      {
        static_cast<void>(connectionId);
        connections.push_back(connection);
      }

      state_->bySession.clear();
      state_->byId.clear();
    }

    for (const auto &connection : connections)
    {
      connection->mark_closed();
    }

    return true;
  }

  bool WebSocketAdapter::attached() const
  {
    std::lock_guard<std::mutex> lock{
        state_->mutex};

    return state_->attached;
  }

  std::size_t
  WebSocketAdapter::connection_count() const
  {
    std::lock_guard<std::mutex> lock{
        state_->mutex};

    return state_->byId.size();
  }

  ConnectionPtr WebSocketAdapter::find_connection(
      const ConnectionId &connectionId) const
  {
    if (connectionId.empty())
    {
      return nullptr;
    }

    std::lock_guard<std::mutex> lock{
        state_->mutex};

    const auto iterator =
        state_->byId.find(connectionId);

    if (iterator == state_->byId.end())
    {
      return nullptr;
    }

    return iterator->second;
  }

  std::vector<ConnectionPtr>
  WebSocketAdapter::connections() const
  {
    std::lock_guard<std::mutex> lock{
        state_->mutex};

    std::vector<ConnectionPtr> result;
    result.reserve(
        state_->byId.size());

    for (const auto &[connectionId, connection] :
         state_->byId)
    {
      static_cast<void>(connectionId);
      result.push_back(connection);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const ConnectionPtr &left,
           const ConnectionPtr &right)
        {
          return left->id() <
                 right->id();
        });

    return result;
  }

  vix::websocket::Server &
  WebSocketAdapter::websocket_server() noexcept
  {
    return websocketServer_;
  }

  const WebSocketAdapterOptions &
  WebSocketAdapter::options() const noexcept
  {
    return state_->options;
  }

} // namespace vix::realtime
