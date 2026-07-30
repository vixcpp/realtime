/**
 *
 * @file server.hpp
 * @author Gaspard Kirira
 * @brief Transport-independent server facade for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_SERVER_HPP
#define VIX_REALTIME_SERVER_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

#include <vix/realtime/api.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/internal/command_queue.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_factory.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_manager.hpp>
#include <vix/realtime/session.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Lifecycle state of a Realtime server facade.
   */
  enum class ServerStatus : std::uint8_t
  {
    /** @brief The server was constructed but has not started. */
    Created = 0,

    /** @brief The server accepts runtime operations. */
    Running,

    /** @brief The server is closing sessions and rooms. */
    Stopping,

    /** @brief The server completed its shutdown. */
    Stopped,

    /** @brief The server encountered an unrecoverable lifecycle failure. */
    Failed
  };

  /**
   * @brief Return the stable textual representation of a server status.
   *
   * @param status Server lifecycle status.
   * @return Stable lowercase identifier.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(ServerStatus status) noexcept
  {
    switch (status)
    {
    case ServerStatus::Created:
      return "created";

    case ServerStatus::Running:
      return "running";

    case ServerStatus::Stopping:
      return "stopping";

    case ServerStatus::Stopped:
      return "stopped";

    case ServerStatus::Failed:
      return "failed";
    }

    return "failed";
  }

  /**
   * @brief Transport-independent facade over a `RoomManager`.
   *
   * A server coordinates the public runtime lifecycle and exposes operations
   * used by transport adapters:
   *
   * - session creation and connection attachment;
   * - temporary connection detachment;
   * - room creation and closure;
   * - room membership;
   * - synchronous and queued commands;
   * - protocol envelope delivery;
   * - stale session and presence cleanup.
   *
   * The server does not listen on sockets. Concrete networking is introduced
   * through transport adapters.
   */
  class VIX_REALTIME_API Server
  {
  public:
    /**
     * @brief Construct a server with default runtime dependencies.
     *
     * @param nodeId Identifier of the local runtime node.
     * @param config Realtime runtime configuration.
     */
    explicit Server(
        NodeId nodeId,
        Config config = {});

    /**
     * @brief Construct a server around an existing room manager.
     *
     * @param manager Room manager used by the server.
     *
     * @throws vix::realtime::Error
     *         When the manager is null.
     */
    explicit Server(RoomManagerPtr manager);

    /**
     * @brief Destroy the server.
     *
     * Destruction performs a best-effort shutdown and never throws.
     */
    ~Server();

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;
    Server(Server &&) = delete;
    Server &operator=(Server &&) = delete;

    /**
     * @brief Start accepting Realtime runtime operations.
     *
     * A stopped server may be started again after its previous rooms and
     * sessions were removed.
     *
     * @return True when the server transitioned to `Running`.
     *
     * @throws vix::realtime::Error
     *         When the server is stopping or failed.
     */
    bool start();

    /**
     * @brief Stop the server and close all managed sessions and rooms.
     *
     * Shutdown is best-effort across all objects. When one or more cleanup
     * operations fail, the server enters `Failed` after attempting every
     * remaining cleanup operation.
     *
     * @return True when a running server entered shutdown.
     *
     * @throws vix::realtime::Error
     *         When one or more sessions or rooms could not be closed.
     */
    bool stop();

    /**
     * @brief Return the current server lifecycle status.
     *
     * @return Server status.
     */
    [[nodiscard]] ServerStatus status() const;

    /**
     * @brief Return whether the server is running.
     *
     * @return True when runtime operations are accepted.
     */
    [[nodiscard]] bool running() const;

    /**
     * @brief Return whether shutdown completed successfully.
     *
     * @return True when the status is `Stopped`.
     */
    [[nodiscard]] bool stopped() const;

    /**
     * @brief Register an application room factory.
     *
     * Factory registration is permitted before or after server startup, but
     * not while stopping or after an unrecoverable failure.
     *
     * @param factory Factory to register.
     * @param replace Whether an existing factory may be replaced.
     * @return True when the registry changed.
     */
    bool register_factory(
        RoomFactoryPtr factory,
        bool replace = false);

    /**
     * @brief Remove one application room factory.
     *
     * @param roomType Registered room type.
     * @return True when a factory was removed.
     */
    bool unregister_factory(
        std::string_view roomType);

    /**
     * @brief Open one process-local room.
     *
     * @param roomId Room identifier.
     * @param roomType Registered application room type.
     * @param metadata Non-authoritative room metadata.
     * @return Open room instance.
     *
     * @throws vix::realtime::Error
     *         When the server is not running or room creation fails.
     */
    [[nodiscard]] RoomPtr open_room(
        RoomId roomId,
        std::string_view roomType,
        JsonObject metadata = {});

    /**
     * @brief Close one process-local room.
     *
     * @param roomId Room identifier.
     * @param remove Whether to remove the closed room from the manager.
     * @return Room close lifecycle result.
     */
    [[nodiscard]] CommandResult close_room(
        const RoomId &roomId,
        bool remove = true);

    /**
     * @brief Find one process-local room.
     *
     * @param roomId Room identifier.
     * @return Room instance, or null.
     */
    [[nodiscard]] RoomPtr find_room(
        const RoomId &roomId) const;

    /**
     * @brief Create one logical session without attaching a connection.
     *
     * @param sessionId Stable logical session identifier.
     * @param identity Application-defined authenticated identity.
     * @param resumeToken Optional session resume token.
     * @param metadata Non-authoritative session metadata.
     * @param now Session creation timestamp.
     * @return Newly created session.
     */
    [[nodiscard]] SessionPtr create_session(
        SessionId sessionId,
        Identity identity = {},
        ResumeToken resumeToken = {},
        JsonObject metadata = {},
        Timestamp now = SystemClock::now());

    /**
     * @brief Create or reuse a session and attach an active connection.
     *
     * When the session already exists, a non-empty supplied identity must match
     * its immutable identity. A non-empty resume token replaces the current
     * token.
     *
     * Any previously attached connection is closed after the replacement.
     *
     * @param sessionId Stable logical session identifier.
     * @param connection Open transport connection.
     * @param identity Identity used when creating or validating the session.
     * @param resumeToken Optional session resume token.
     * @param metadata Session metadata used when creating the session.
     * @param now Connection attachment timestamp.
     * @return Connected logical session.
     *
     * @throws vix::realtime::Error
     *         When the server, session, identity, or connection is invalid.
     */
    [[nodiscard]] SessionPtr connect(
        SessionId sessionId,
        ConnectionPtr connection,
        Identity identity = {},
        ResumeToken resumeToken = {},
        JsonObject metadata = {},
        Timestamp now = SystemClock::now());

    /**
     * @brief Detach one specific transport connection.
     *
     * The logical session and its room memberships remain available for
     * resumption.
     *
     * @param sessionId Logical session identifier.
     * @param connectionId Expected active connection identifier.
     * @param now Detachment timestamp.
     * @return Detached connection, or null when it no longer matches.
     */
    [[nodiscard]] ConnectionPtr disconnect(
        const SessionId &sessionId,
        const ConnectionId &connectionId,
        Timestamp now = SystemClock::now());

    /**
     * @brief Permanently close and remove one logical session.
     *
     * @param sessionId Logical session identifier.
     * @param code Transport close reason.
     * @param reason Optional human-readable close reason.
     * @param now Session closure timestamp.
     * @return True when a session was closed.
     */
    bool close_session(
        const SessionId &sessionId,
        ErrorCode code = ErrorCode::Cancelled,
        std::string_view reason = {},
        Timestamp now = SystemClock::now());

    /**
     * @brief Find one logical session.
     *
     * @param sessionId Logical session identifier.
     * @return Session instance, or null.
     */
    [[nodiscard]] SessionPtr find_session(
        const SessionId &sessionId) const;

    /**
     * @brief Join a logical session to an open room.
     *
     * @param sessionId Joining session.
     * @param roomId Target room.
     * @param now Membership timestamp.
     * @return Room join result.
     */
    [[nodiscard]] CommandResult join_room(
        const SessionId &sessionId,
        const RoomId &roomId,
        Timestamp now = SystemClock::now());

    /**
     * @brief Remove a logical session from a room.
     *
     * @param sessionId Leaving session.
     * @param roomId Target room.
     * @param now Membership update timestamp.
     * @return Room leave result.
     */
    [[nodiscard]] CommandResult leave_room(
        const SessionId &sessionId,
        const RoomId &roomId,
        Timestamp now = SystemClock::now());

    /**
     * @brief Execute one room command synchronously.
     *
     * @param command Room command.
     * @return Command execution result.
     */
    [[nodiscard]] CommandResult execute(
        const RoomCommand &command);

    /**
     * @brief Enqueue one room command.
     *
     * @param command Room command.
     * @return Queue operation status.
     */
    [[nodiscard]] internal::CommandQueueStatus enqueue(
        RoomCommand command);

    /**
     * @brief Process the oldest pending command of one room.
     *
     * @param roomId Room identifier.
     * @return Command result, or no value when the queue is empty.
     */
    [[nodiscard]] std::optional<CommandResult> process_next(
        const RoomId &roomId);

    /**
     * @brief Send one protocol envelope to a logical session.
     *
     * @param sessionId Destination logical session.
     * @param envelope Protocol envelope to send.
     *
     * @throws vix::realtime::Error
     *         When the session has no active connection.
     */
    void send(
        const SessionId &sessionId,
        const protocol::Envelope &envelope) const;

    /**
     * @brief Remove detached sessions whose resume window elapsed.
     *
     * Connected sessions are preserved. When session resumption is disabled,
     * every detached session is eligible for removal.
     *
     * Cleanup continues when an individual session cannot be closed.
     *
     * @param now Current timestamp.
     * @return Number of removed sessions.
     */
    std::size_t prune_expired_sessions(
        Timestamp now = SystemClock::now());

    /**
     * @brief Remove stale presence records using the configured timeout.
     *
     * @param now Current timestamp.
     * @return Number of removed presence records.
     */
    std::size_t prune_stale_presence(
        Timestamp now = SystemClock::now());

    /**
     * @brief Return the underlying room manager.
     *
     * @return Shared room manager.
     */
    [[nodiscard]] const RoomManagerPtr &
    manager() const noexcept;

    /**
     * @brief Return the local runtime node identifier.
     *
     * @return Node identifier.
     */
    [[nodiscard]] const NodeId &node_id() const noexcept;

    /**
     * @brief Return the Realtime configuration.
     *
     * @return Configuration reference.
     */
    [[nodiscard]] const Config &config() const noexcept;

  private:
    /**
     * @brief Ensure the server currently accepts runtime operations.
     *
     * @throws vix::realtime::Error
     *         When the server is not running.
     */
    void require_running() const;

    /**
     * @brief Ensure registry mutations are currently permitted.
     *
     * @throws vix::realtime::Error
     *         When the server is stopping or failed.
     */
    void require_registry_available() const;

    /** @brief Central room and session runtime manager. */
    RoomManagerPtr manager_{};

    /** @brief Protects the server lifecycle state. */
    mutable std::mutex mutex_{};

    /** @brief Current server lifecycle status. */
    ServerStatus status_{ServerStatus::Created};
  };

  /**
   * @brief Shared ownership pointer for a Realtime server.
   */
  using ServerPtr = std::shared_ptr<Server>;

} // namespace vix::realtime

#endif // VIX_REALTIME_SERVER_HPP
