/**
 *
 * @file room_manager.hpp
 * @author Gaspard Kirira
 * @brief Central room and session orchestrator for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_MANAGER_HPP
#define VIX_REALTIME_ROOM_MANAGER_HPP

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/event_store.hpp>
#include <vix/realtime/internal/command_queue.hpp>
#include <vix/realtime/internal/event_dispatcher.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/presence.hpp>
#include <vix/realtime/presence_store.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_directory.hpp>
#include <vix/realtime/room_factory.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/session.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/snapshot_store.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Central orchestrator for rooms, sessions, presence, and delivery.
   *
   * A room manager owns the process-local runtime collections and coordinates:
   *
   * - room factory registration;
   * - room creation, opening, and shutdown;
   * - logical session creation and connection attachment;
   * - room membership consistency;
   * - process-local room ownership;
   * - logical presence updates;
   * - authoritative event delivery to active sessions.
   *
   * The manager does not own application state definitions. Those are created
   * by registered `RoomFactory` implementations.
   */
  class VIX_REALTIME_API RoomManager
  {
  public:
    /**
     * @brief Construct a manager with in-memory stores.
     *
     * This constructor creates:
     *
     * - `MemoryEventStore`;
     * - `MemorySnapshotStore`;
     * - `LocalPresenceStore` when presence is enabled;
     * - `RoomDirectory`.
     *
     * @param nodeId Identifier of the local runtime node.
     * @param config Realtime runtime configuration.
     *
     * @throws vix::realtime::Error
     *         When the node or configuration is invalid.
     */
    explicit RoomManager(
        NodeId nodeId,
        Config config = {});

    /**
     * @brief Construct a manager with explicit runtime dependencies.
     *
     * @param nodeId Identifier of the local runtime node.
     * @param config Realtime runtime configuration.
     * @param eventStore Authoritative room event store.
     * @param snapshotStore Optional room snapshot store.
     * @param presenceStore Optional presence store.
     * @param roomDirectory Room ownership directory.
     *
     * @throws vix::realtime::Error
     *         When required dependencies or configuration are invalid.
     */
    RoomManager(
        NodeId nodeId,
        Config config,
        EventStorePtr eventStore,
        SnapshotStorePtr snapshotStore,
        PresenceStorePtr presenceStore,
        std::shared_ptr<RoomDirectory> roomDirectory);

    /**
     * @brief Destroy the room manager and close local runtime objects.
     */
    ~RoomManager();

    RoomManager(const RoomManager &) = delete;
    RoomManager &operator=(const RoomManager &) = delete;
    RoomManager(RoomManager &&) = delete;
    RoomManager &operator=(RoomManager &&) = delete;

    /**
     * @brief Register an application room factory.
     *
     * @param factory Factory to register.
     * @param replace Whether an existing factory with the same type may be
     *                replaced.
     * @return True when the registry changed.
     *
     * @throws vix::realtime::Error
     *         When the factory or its room type is invalid.
     */
    bool register_factory(
        RoomFactoryPtr factory,
        bool replace = false);

    /**
     * @brief Remove one room factory from the registry.
     *
     * Existing rooms keep their already-created state and handlers.
     *
     * @param roomType Registered room type.
     * @return True when a factory was removed.
     */
    bool unregister_factory(
        std::string_view roomType);

    /**
     * @brief Find a registered room factory.
     *
     * @param roomType Registered room type.
     * @return Factory, or null when absent.
     */
    [[nodiscard]] RoomFactoryPtr find_factory(
        std::string_view roomType) const;

    /**
     * @brief Return whether a room factory is registered.
     *
     * @param roomType Room type to inspect.
     * @return True when a matching factory exists.
     */
    [[nodiscard]] bool has_factory(
        std::string_view roomType) const;

    /**
     * @brief Return all registered room types.
     *
     * Results are sorted lexicographically.
     *
     * @return Registered room type identifiers.
     */
    [[nodiscard]] std::vector<std::string>
    factory_types() const;

    /**
     * @brief Create and open one local room.
     *
     * The manager:
     *
     * - resolves the registered room factory;
     * - acquires process-local room ownership;
     * - creates the state and handler;
     * - constructs the room;
     * - restores and opens it.
     *
     * @param roomId Room identifier.
     * @param roomType Registered application room type.
     * @param metadata Non-authoritative room metadata.
     * @return Open room instance.
     *
     * @throws vix::realtime::Error
     *         When limits, ownership, factory creation, or room opening fail.
     */
    [[nodiscard]] RoomPtr open_room(
        RoomId roomId,
        std::string_view roomType,
        JsonObject metadata = {});

    /**
     * @brief Close one local room.
     *
     * When the close callback succeeds, session memberships, presence, local
     * ownership, and optionally the manager room entry are removed.
     *
     * @param roomId Room identifier.
     * @param remove Whether to remove the room from the manager after closing.
     * @return Close lifecycle result.
     *
     * @throws vix::realtime::Error
     *         When the room is not found or cannot close.
     */
    [[nodiscard]] CommandResult close_room(
        const RoomId &roomId,
        bool remove = true);

    /**
     * @brief Find one process-local room.
     *
     * @param roomId Room identifier.
     * @return Room, or null when absent.
     */
    [[nodiscard]] RoomPtr find_room(
        const RoomId &roomId) const;

    /**
     * @brief Return one process-local room.
     *
     * @param roomId Room identifier.
     * @return Room instance.
     *
     * @throws vix::realtime::Error
     *         When the room is not found.
     */
    [[nodiscard]] RoomPtr require_room(
        const RoomId &roomId) const;

    /**
     * @brief Return whether one room is managed locally.
     *
     * @param roomId Room identifier.
     * @return True when the room exists in the local manager.
     */
    [[nodiscard]] bool has_room(
        const RoomId &roomId) const;

    /**
     * @brief Return all process-local room identifiers.
     *
     * Results are sorted by room identifier.
     *
     * @return Managed room identifiers.
     */
    [[nodiscard]] std::vector<RoomId> room_ids() const;

    /**
     * @brief Return the number of managed rooms.
     *
     * @return Process-local room count.
     */
    [[nodiscard]] std::size_t room_count() const;

    /**
     * @brief Create one logical session.
     *
     * The session initially has no active connection.
     *
     * @param sessionId Stable logical session identifier.
     * @param identity Application-defined authenticated identity.
     * @param resumeToken Optional session resume token.
     * @param metadata Non-authoritative session metadata.
     * @param now Session creation timestamp.
     * @return Newly created logical session.
     *
     * @throws vix::realtime::Error
     *         When the identifier already exists or session limits are reached.
     */
    [[nodiscard]] SessionPtr create_session(
        SessionId sessionId,
        Identity identity = {},
        ResumeToken resumeToken = {},
        JsonObject metadata = {},
        Timestamp now = SystemClock::now());

    /**
     * @brief Find one logical session.
     *
     * @param sessionId Logical session identifier.
     * @return Session, or null when absent.
     */
    [[nodiscard]] SessionPtr find_session(
        const SessionId &sessionId) const;

    /**
     * @brief Return one logical session.
     *
     * @param sessionId Logical session identifier.
     * @return Session instance.
     *
     * @throws vix::realtime::Error
     *         When the session is not found.
     */
    [[nodiscard]] SessionPtr require_session(
        const SessionId &sessionId) const;

    /**
     * @brief Return whether one logical session exists.
     *
     * @param sessionId Logical session identifier.
     * @return True when the session is managed locally.
     */
    [[nodiscard]] bool has_session(
        const SessionId &sessionId) const;

    /**
     * @brief Return all managed session identifiers.
     *
     * Results are sorted by session identifier.
     *
     * @return Managed session identifiers.
     */
    [[nodiscard]] std::vector<SessionId> session_ids() const;

    /**
     * @brief Return the number of managed logical sessions.
     *
     * @return Session count.
     */
    [[nodiscard]] std::size_t session_count() const;

    /**
     * @brief Attach a transport connection to a logical session.
     *
     * Existing room presence records are updated to reflect the new
     * connection. The previous connection is returned but not closed.
     *
     * @param sessionId Logical session identifier.
     * @param connection Open transport connection.
     * @param now Attachment timestamp.
     * @return Previously attached connection, or null.
     *
     * @throws vix::realtime::Error
     *         When the session or connection is invalid.
     */
    [[nodiscard]] ConnectionPtr attach_connection(
        const SessionId &sessionId,
        ConnectionPtr connection,
        Timestamp now = SystemClock::now());

    /**
     * @brief Detach a specific connection from a logical session.
     *
     * Presence records are marked detached only when the connection ID matches
     * the currently attached connection.
     *
     * @param sessionId Logical session identifier.
     * @param connectionId Expected active connection identifier.
     * @param now Detachment timestamp.
     * @return Detached connection, or null when the identifier did not match.
     *
     * @throws vix::realtime::Error
     *         When the session is not found.
     */
    [[nodiscard]] ConnectionPtr detach_connection(
        const SessionId &sessionId,
        const ConnectionId &connectionId,
        Timestamp now = SystemClock::now());

    /**
     * @brief Permanently close and remove a logical session.
     *
     * The manager first removes every room membership. A rejected room leave
     * prevents final session removal.
     *
     * @param sessionId Logical session identifier.
     * @param code Transport close reason.
     * @param reason Optional human-readable close reason.
     * @param now Session closure timestamp.
     * @return True when an existing session was closed.
     *
     * @throws vix::realtime::Error
     *         When one room membership cannot be removed.
     */
    bool close_session(
        const SessionId &sessionId,
        ErrorCode code = ErrorCode::Cancelled,
        std::string_view reason = {},
        Timestamp now = SystemClock::now());

    /**
     * @brief Join a logical session to an open room.
     *
     * Session membership and presence are prepared before invoking the room
     * lifecycle callback. They are rolled back when the room rejects or fails
     * the join.
     *
     * @param sessionId Joining logical session.
     * @param roomId Target room.
     * @param now Membership timestamp.
     * @return Room join result.
     *
     * @throws vix::realtime::Error
     *         When the session, room, capacity, or membership is invalid.
     */
    [[nodiscard]] CommandResult join_room(
        const SessionId &sessionId,
        const RoomId &roomId,
        Timestamp now = SystemClock::now());

    /**
     * @brief Remove a logical session from a room.
     *
     * Session membership and presence are updated only when the room lifecycle
     * callback does not reject the leave.
     *
     * @param sessionId Leaving logical session.
     * @param roomId Target room.
     * @param now Membership update timestamp.
     * @return Room leave result.
     *
     * @throws vix::realtime::Error
     *         When the session, room, or membership is invalid.
     */
    [[nodiscard]] CommandResult leave_room(
        const SessionId &sessionId,
        const RoomId &roomId,
        Timestamp now = SystemClock::now());

    /**
     * @brief Execute one command synchronously in its target room.
     *
     * @param command Room command.
     * @return Command execution result.
     *
     * @throws vix::realtime::Error
     *         When the room, session, or membership is invalid.
     */
    [[nodiscard]] CommandResult execute(
        const RoomCommand &command);

    /**
     * @brief Enqueue one command in its target room.
     *
     * @param command Room command.
     * @return Queue operation status.
     *
     * @throws vix::realtime::Error
     *         When the room, session, or membership is invalid.
     */
    [[nodiscard]] internal::CommandQueueStatus enqueue(
        RoomCommand command);

    /**
     * @brief Process the next pending command of one room.
     *
     * @param roomId Room identifier.
     * @return Command result, or no value when the queue is empty.
     *
     * @throws vix::realtime::Error
     *         When the room is not found or processing fails.
     */
    [[nodiscard]] std::optional<CommandResult> process_next(
        const RoomId &roomId);

    /**
     * @brief Find one stored room presence.
     *
     * @param roomId Room identifier.
     * @param sessionId Logical session identifier.
     * @return Presence record, or no value.
     */
    [[nodiscard]] std::optional<Presence> find_presence(
        const RoomId &roomId,
        const SessionId &sessionId) const;

    /**
     * @brief List stored presence records for one room.
     *
     * @param roomId Room identifier.
     * @return Room presence records.
     */
    [[nodiscard]] std::vector<Presence> room_presence(
        const RoomId &roomId) const;

    /**
     * @brief Return the local runtime node identifier.
     *
     * @return Node identifier.
     */
    [[nodiscard]] const NodeId &node_id() const noexcept;

    /**
     * @brief Return the manager configuration.
     *
     * @return Constant configuration reference.
     */
    [[nodiscard]] const Config &config() const noexcept;

    /**
     * @brief Return the configured event store.
     *
     * @return Event store.
     */
    [[nodiscard]] const EventStorePtr &
    event_store() const noexcept;

    /**
     * @brief Return the configured snapshot store.
     *
     * @return Snapshot store, or null when disabled.
     */
    [[nodiscard]] const SnapshotStorePtr &
    snapshot_store() const noexcept;

    /**
     * @brief Return the configured presence store.
     *
     * @return Presence store, or null when disabled.
     */
    [[nodiscard]] const PresenceStorePtr &
    presence_store() const noexcept;

    /**
     * @brief Return the room ownership directory.
     *
     * @return Room directory.
     */
    [[nodiscard]] const std::shared_ptr<RoomDirectory> &
    room_directory() const noexcept;

    /**
     * @brief Return the event dispatcher shared by local rooms.
     *
     * @return Event dispatcher.
     */
    [[nodiscard]] const std::shared_ptr<internal::EventDispatcher> &
    event_dispatcher() const noexcept;

  private:
    /**
     * @brief Deliver one committed room event to a logical session.
     */
    void deliver_event(
        const SessionId &sessionId,
        const RoomEvent &event) const;

    /**
     * @brief Update every joined presence after connection attachment.
     */
    void mark_session_present(
        const SessionPtr &session,
        Timestamp now) noexcept;

    /**
     * @brief Update every joined presence after connection detachment.
     */
    void mark_session_detached(
        const SessionPtr &session,
        Timestamp now) noexcept;

    /**
     * @brief Update one presence activity timestamp without failing commands.
     */
    void touch_presence(
        const RoomId &roomId,
        const SessionId &sessionId,
        Timestamp now = SystemClock::now()) noexcept;

    /**
     * @brief Remove one room from the manager when the pointer still matches.
     */
    void erase_room_if_same(
        const RoomId &roomId,
        const RoomPtr &room);

    /**
     * @brief Remove one session when the pointer still matches.
     */
    void erase_session_if_same(
        const SessionId &sessionId,
        const SessionPtr &session);

    /** @brief Identifier of the process-local runtime node. */
    NodeId nodeId_{};

    /** @brief Realtime runtime configuration. */
    Config config_{};

    /** @brief Persistent authoritative event store. */
    EventStorePtr eventStore_{};

    /** @brief Optional persistent snapshot store. */
    SnapshotStorePtr snapshotStore_{};

    /** @brief Optional logical presence store. */
    PresenceStorePtr presenceStore_{};

    /** @brief Room ownership coordination directory. */
    std::shared_ptr<RoomDirectory> roomDirectory_{};

    /** @brief Event dispatcher shared by every local room. */
    std::shared_ptr<internal::EventDispatcher> eventDispatcher_{};

    /** @brief Protects registries, rooms, and sessions. */
    mutable std::mutex mutex_{};

    /** @brief Registered room factories indexed by room type. */
    std::unordered_map<std::string, RoomFactoryPtr> factories_{};

    /** @brief Process-local rooms indexed by room identifier. */
    std::unordered_map<RoomId, RoomPtr> rooms_{};

    /** @brief Logical sessions indexed by session identifier. */
    std::unordered_map<SessionId, SessionPtr> sessions_{};
  };

  /**
   * @brief Shared ownership pointer for a room manager.
   */
  using RoomManagerPtr = std::shared_ptr<RoomManager>;

} // namespace vix::realtime

#endif // VIX_REALTIME_ROOM_MANAGER_HPP
