/**
 *
 * @file room.hpp
 * @author Gaspard Kirira
 * @brief Stateful authoritative room runtime for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_HPP
#define VIX_REALTIME_ROOM_HPP

#include <cstddef>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/command_queue_status.hpp>
#include <vix/realtime/config.hpp>
#include <vix/realtime/event_store.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_factory.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/snapshot_store.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace internal
  {
    class EventDispatcher;
    struct RoomRuntime;
  }

  class Session;
  using SessionPtr = std::shared_ptr<Session>;

  /**
   * @brief Lifecycle state of an authoritative room runtime.
   */
  enum class RoomStatus : std::uint8_t
  {
    /** @brief The room was created but has not been opened. */
    Created = 0,

    /** @brief The room is restoring state and running its open callback. */
    Opening,

    /** @brief The room is available for commands and memberships. */
    Open,

    /** @brief The room is running its close callback. */
    Closing,

    /** @brief The room completed its shutdown. */
    Closed,

    /** @brief The room encountered an unrecoverable state failure. */
    Failed
  };

  /**
   * @brief Return the stable textual representation of a room status.
   *
   * @param status Room lifecycle status.
   * @return Stable lowercase identifier.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(RoomStatus status) noexcept
  {
    switch (status)
    {
    case RoomStatus::Created:
      return "created";

    case RoomStatus::Opening:
      return "opening";

    case RoomStatus::Open:
      return "open";

    case RoomStatus::Closing:
      return "closing";

    case RoomStatus::Closed:
      return "closed";

    case RoomStatus::Failed:
      return "failed";
    }

    return "failed";
  }

  /**
   * @brief Authoritative runtime for one logical room.
   *
   * A room owns:
   *
   * - one application-defined authoritative state;
   * - one application-defined command handler;
   * - one bounded FIFO command queue;
   * - logical session memberships;
   * - the current room version and event position;
   * - snapshot scheduling state.
   *
   * All commands for one room are processed serially. Produced events are
   * persisted before they are applied to the authoritative state.
   */
  class VIX_REALTIME_API Room
  {
  public:
    /**
     * @brief Construct an authoritative room runtime.
     *
     * @param roomId Stable room identifier.
     * @param roomType Registered application room type.
     * @param components State and handler created by the room factory.
     * @param eventStore Persistent authoritative event store.
     * @param snapshotStore Optional room snapshot store.
     * @param config Realtime runtime configuration.
     * @param eventDispatcher Optional transport-independent event dispatcher.
     * @param ownerNodeId Optional node currently owning the room.
     * @param metadata Non-authoritative room metadata.
     *
     * @throws vix::realtime::Error
     *         When identifiers, components, stores, or configuration are
     *         invalid.
     */
    Room(
        RoomId roomId,
        std::string roomType,
        RoomComponents components,
        EventStorePtr eventStore,
        SnapshotStorePtr snapshotStore,
        Config config = {},
        std::optional<NodeId> ownerNodeId = std::nullopt,
        JsonObject metadata = {});

    Room(
        RoomId roomId,
        RoomStatePtr state,
        RoomHandlerPtr handler,
        EventStorePtr eventStore,
        SnapshotStorePtr snapshotStore,
        Config config = {});

    /**
     * @brief Destroy the room.
     */
    ~Room();

    Room(const Room &) = delete;
    Room &operator=(const Room &) = delete;
    Room(Room &&) = delete;
    Room &operator=(Room &&) = delete;

    /**
     * @brief Open the room and restore its authoritative state.
     *
     * The runtime loads the latest snapshot, replays subsequent events, and
     * invokes `RoomHandler::on_open()`.
     *
     * @return Open lifecycle result.
     *
     * @throws vix::realtime::Error
     *         When restoration, replay, or state application fails.
     */
    [[nodiscard]] CommandResult open();

    /**
     * @brief Close the room.
     *
     * The close callback runs before the optional final snapshot is created.
     * A rejected close result leaves the room open.
     *
     * @return Close lifecycle result.
     *
     * @throws vix::realtime::Error
     *         When persistence or state application fails.
     */
    [[nodiscard]] CommandResult close();

    /**
     * @brief Enqueue one room command without waiting.
     *
     * @param command Command to enqueue.
     * @return Queue operation status.
     *
     * @throws vix::realtime::Error
     *         When the room or command is invalid.
     */
    [[nodiscard]] CommandQueueStatus enqueue(
        RoomCommand command);

    /**
     * @brief Process the oldest queued command.
     *
     * @return Command result, or no value when no command is available.
     *
     * @throws vix::realtime::Error
     *         When command processing fails.
     */
    [[nodiscard]] std::optional<CommandResult>
    process_next();

    /**
     * @brief Execute one room command synchronously.
     *
     * The command bypasses the pending queue but still uses the same serialized
     * room execution lock.
     *
     * @param command Command to execute.
     * @return Command result containing persisted events.
     *
     * @throws vix::realtime::Error
     *         When the room is unavailable or processing fails.
     */
    [[nodiscard]] CommandResult execute(
        const RoomCommand &command);

    [[nodiscard]] CommandResult command(
        const RoomCommand &command)
    {
      return execute(command);
    }

    [[nodiscard]] CommandResult process_command(
        const RoomCommand &command)
    {
      return execute(command);
    }

    [[nodiscard]] CommandResult execute_command(
        const RoomCommand &command)
    {
      return execute(command);
    }

    /**
     * @brief Join a logical session to the room.
     *
     * The membership is inserted only when the lifecycle callback is not
     * rejected.
     *
     * @param sessionId Joining session.
     * @return Join lifecycle result.
     *
     * @throws vix::realtime::Error
     *         When the room is unavailable or full.
     */
    [[nodiscard]] CommandResult join(
        const SessionId &sessionId);

    [[nodiscard]] CommandResult join(
        const SessionPtr &session);

    [[nodiscard]] CommandResult join_session(
        const SessionPtr &session)
    {
      return join(session);
    }

    [[nodiscard]] CommandResult add_session(
        const SessionPtr &session)
    {
      return join(session);
    }

    [[nodiscard]] CommandResult add_member(
        const SessionPtr &session)
    {
      return join(session);
    }

    /**
     * @brief Remove a logical session from the room.
     *
     * A rejected lifecycle result preserves the existing membership.
     *
     * @param sessionId Leaving session.
     * @return Leave lifecycle result.
     *
     * @throws vix::realtime::Error
     *         When the room or membership is unavailable.
     */
    [[nodiscard]] CommandResult leave(
        const SessionId &sessionId);

    void broadcast(
        const RoomEvent &event) const;

    void broadcast_event(
        const RoomEvent &event) const
    {
      broadcast(event);
    }

    void publish_event(
        const RoomEvent &event) const
    {
      broadcast(event);
    }

    void emit(
        const RoomEvent &event) const
    {
      broadcast(event);
    }

    /**
     * @brief Create and persist a room snapshot.
     *
     * When `force` is false, the configured snapshot policy determines whether
     * a new snapshot is required.
     *
     * @param force Whether to bypass the normal snapshot interval.
     * @return Persisted snapshot, or no value when no snapshot was required.
     *
     * @throws vix::realtime::Error
     *         When snapshot storage is unavailable or persistence fails.
     */
    [[nodiscard]] std::optional<RoomSnapshot>
    snapshot(bool force = true);

    /**
     * @brief Return the room identifier.
     *
     * @return Room identifier.
     */
    [[nodiscard]] const RoomId &id() const noexcept;

    /**
     * @brief Return the registered application room type.
     *
     * @return Room type.
     */
    [[nodiscard]] const std::string &
    type() const noexcept;

    /**
     * @brief Return the current room lifecycle status.
     *
     * @return Room status.
     */
    [[nodiscard]] RoomStatus status() const;

    /**
     * @brief Return whether the room is currently open.
     *
     * @return True when commands and memberships are accepted.
     */
    [[nodiscard]] bool is_open() const;

    [[nodiscard]] bool is_closed() const;

    /**
     * @brief Return whether the room entered an unrecoverable failure state.
     *
     * @return True when the room status is `Failed`.
     */
    [[nodiscard]] bool failed() const;

    /**
     * @brief Return the current authoritative room version.
     *
     * @return Current room version.
     */
    [[nodiscard]] RoomVersion version() const;

    /**
     * @brief Return the latest persisted event identifier.
     *
     * @return Latest event ID.
     */
    [[nodiscard]] EventId last_event_id() const;

    /**
     * @brief Return a serialized copy of the authoritative state.
     *
     * @return Serialized authoritative state.
     */
    [[nodiscard]] JsonObject serialize_state() const;

    [[nodiscard]] const RoomState &state() const;

    [[nodiscard]] Config config() const;

    /**
     * @brief Return whether a session currently belongs to the room.
     *
     * @param sessionId Session identifier.
     * @return True when the membership exists.
     */
    [[nodiscard]] bool has_session(
        const SessionId &sessionId) const;

    /**
     * @brief Return all current session memberships.
     *
     * @return Session identifiers in deterministic order.
     */
    [[nodiscard]] std::vector<SessionId>
    sessions() const;

    /**
     * @brief Return the number of current session memberships.
     *
     * @return Session count.
     */
    [[nodiscard]] std::size_t session_count() const;

    [[nodiscard]] std::size_t member_count() const;

    [[nodiscard]] bool empty() const;

    /**
     * @brief Return the number of pending commands.
     *
     * @return Pending command count.
     */
    [[nodiscard]] std::size_t pending_command_count() const;

    /**
     * @brief Return the latest room activity timestamp.
     *
     * @return Latest activity timestamp.
     */
    [[nodiscard]] Timestamp last_activity_at() const;

    /**
     * @brief Return the optional node currently owning the room.
     *
     * @return Owner node identifier.
     */
    [[nodiscard]] std::optional<NodeId>
    owner_node_id() const;

    /**
     * @brief Assign the node currently owning the room.
     *
     * @param nodeId Owner node identifier.
     */
    void set_owner_node_id(NodeId nodeId);

    /**
     * @brief Remove the explicit owner node identifier.
     */
    void clear_owner_node_id();

    /**
     * @brief Return a copy of non-authoritative room metadata.
     *
     * @return Room metadata.
     */
    [[nodiscard]] JsonObject metadata() const;

    /**
     * @brief Replace non-authoritative room metadata.
     *
     * @param value New room metadata.
     */
    void set_metadata(JsonObject value);

  private:
    friend class RoomManager;

    /** @brief Configure transport-independent delivery for manager-owned rooms. */
    void set_event_dispatcher(
        std::shared_ptr<internal::EventDispatcher> dispatcher);

    /**
     * @brief Build an execution context while the room lock is held.
     */
    [[nodiscard]] RoomContext make_context_locked(
        std::optional<SessionId> sessionId,
        RequestId requestId = {},
        CorrelationId correlationId = {},
        Timestamp now = SystemClock::now()) const;

    /**
     * @brief Normalize, persist, and apply accepted result events.
     */
    [[nodiscard]] std::vector<RoomEvent>
    commit_result_locked(
        CommandResult &result,
        const RoomContext &context);

    /**
     * @brief Restore the room from snapshots and persisted events.
     */
    void restore_locked();

    /**
     * @brief Apply one persisted replay event.
     */
    void apply_replay_event_locked(
        const RoomEvent &event);

    /**
     * @brief Persist a snapshot while the room lock is held.
     */
    [[nodiscard]] std::optional<RoomSnapshot>
    save_snapshot_locked(
        bool force,
        bool roomClosing);

    /**
     * @brief Attempt automatic snapshot creation without failing a command.
     */
    void try_automatic_snapshot_locked() noexcept;

    /**
     * @brief Deliver committed events outside the room lock.
     */
    void dispatch_events(
        const std::vector<RoomEvent> &events,
        const std::vector<SessionId> &roomSessions,
        const std::shared_ptr<internal::EventDispatcher> &dispatcher) const noexcept;

    /**
     * @brief Return the current session collection while locked.
     */
    [[nodiscard]] std::vector<SessionId>
    sessions_locked() const;

    /**
     * @brief Update the latest room activity timestamp.
     */
    void touch_locked(
        Timestamp now = SystemClock::now()) noexcept;

    /** @brief Stable room identifier. */
    RoomId roomId_{};

    /** @brief Registered application room type. */
    std::string roomType_{};

    /** @brief Authoritative application room state. */
    RoomStatePtr state_{};

    /** @brief Application room behavior. */
    RoomHandlerPtr handler_{};

    /** @brief Persistent authoritative event store. */
    EventStorePtr eventStore_{};

    /** @brief Optional persistent snapshot store. */
    SnapshotStorePtr snapshotStore_{};

    /** @brief Realtime runtime configuration. */
    Config config_{};

    /** @brief Private command queue and snapshot scheduling state. */
    std::unique_ptr<internal::RoomRuntime> runtime_;

    /** @brief Direct command executions currently occupying queue capacity. */
    std::atomic<std::size_t> directCommandsInFlight_{0};

    /** @brief Transport-independent event delivery. */
    std::shared_ptr<internal::EventDispatcher> eventDispatcher_{};

    /** @brief Protects room state, lifecycle, and memberships. */
    mutable std::mutex mutex_{};

    /** @brief Current room lifecycle status. */
    RoomStatus status_{RoomStatus::Created};

    /** @brief Current authoritative room version. */
    RoomVersion roomVersion_{};

    /** @brief Latest persisted event identifier. */
    EventId lastEventId_{};

    /** @brief Current logical room memberships. */
    std::set<SessionId> sessions_{};

    /** @brief Optional sessions retained for direct room-level broadcasts. */
    std::unordered_map<SessionId, SessionPtr> sessionRefs_{};

    /** @brief Optional runtime node currently owning the room. */
    std::optional<NodeId> ownerNodeId_{};

    /** @brief Latest room activity timestamp. */
    Timestamp lastActivityAt_{SystemClock::now()};

    /** @brief Non-authoritative room metadata. */
    JsonObject metadata_{};
  };

  /**
   * @brief Shared ownership pointer for an authoritative room.
   */
  using RoomPtr = std::shared_ptr<Room>;

  /**
   * @brief Weak ownership pointer for an authoritative room.
   */
  using WeakRoomPtr = std::weak_ptr<Room>;

} // namespace vix::realtime

#endif // VIX_REALTIME_ROOM_HPP
