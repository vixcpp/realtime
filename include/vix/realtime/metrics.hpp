/**
 *
 * @file metrics.hpp
 * @author Gaspard Kirira
 * @brief Thread-safe runtime metrics for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_METRICS_HPP
#define VIX_REALTIME_METRICS_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <vix/realtime/api.hpp>
#include <vix/realtime/command_result.hpp>

namespace vix::realtime
{
  /**
   * @brief Immutable snapshot of Realtime runtime metrics.
   *
   * Gauge values describe the runtime when the snapshot was captured.
   * Cumulative values describe activity since the latest metrics reset.
   */
  struct MetricsSnapshot
  {
    /** @brief Rooms currently managed by the runtime. */
    std::uint64_t activeRooms{0};

    /** @brief Logical sessions currently managed by the runtime. */
    std::uint64_t activeSessions{0};

    /** @brief Sessions currently attached to transport connections. */
    std::uint64_t attachedConnections{0};

    /** @brief Commands currently waiting in room queues. */
    std::uint64_t queuedCommands{0};

    /** @brief Presence records currently considered active. */
    std::uint64_t activePresence{0};

    /** @brief Rooms successfully opened. */
    std::uint64_t roomsOpened{0};

    /** @brief Rooms successfully closed. */
    std::uint64_t roomsClosed{0};

    /** @brief Logical sessions created. */
    std::uint64_t sessionsCreated{0};

    /** @brief Logical sessions permanently closed. */
    std::uint64_t sessionsClosed{0};

    /** @brief Transport connections attached to sessions. */
    std::uint64_t connectionsAttached{0};

    /** @brief Transport connections detached from sessions. */
    std::uint64_t connectionsDetached{0};

    /** @brief Commands successfully inserted into room queues. */
    std::uint64_t commandsEnqueued{0};

    /** @brief Commands processed by room handlers. */
    std::uint64_t commandsProcessed{0};

    /** @brief Commands accepted by room handlers. */
    std::uint64_t commandsAccepted{0};

    /** @brief Commands rejected by room handlers. */
    std::uint64_t commandsRejected{0};

    /** @brief Commands ignored by room handlers. */
    std::uint64_t commandsIgnored{0};

    /** @brief Persisted authoritative room events. */
    std::uint64_t eventsPersisted{0};

    /** @brief Event dispatch operations attempted. */
    std::uint64_t eventDispatches{0};

    /** @brief Total sessions selected as event recipients. */
    std::uint64_t eventRecipients{0};

    /** @brief Successful event deliveries. */
    std::uint64_t eventDeliveriesSucceeded{0};

    /** @brief Failed event deliveries. */
    std::uint64_t eventDeliveriesFailed{0};

    /** @brief Room snapshots successfully persisted. */
    std::uint64_t snapshotsCreated{0};

    /** @brief Room snapshots restored into state. */
    std::uint64_t snapshotsRestored{0};

    /** @brief Replay operations completed successfully. */
    std::uint64_t replayOperations{0};

    /** @brief Events applied during room replay. */
    std::uint64_t replayEventsApplied{0};

    /** @brief Serialized event bytes processed during replay. */
    std::uint64_t replayBytes{0};

    /** @brief Session resumption attempts. */
    std::uint64_t resumeAttempts{0};

    /** @brief Successful session resumptions. */
    std::uint64_t resumeSucceeded{0};

    /** @brief Failed session resumptions. */
    std::uint64_t resumeFailed{0};

    /** @brief Logical room presence joins. */
    std::uint64_t presenceJoins{0};

    /** @brief Logical room presence leaves. */
    std::uint64_t presenceLeaves{0};

    /** @brief Transport messages received. */
    std::uint64_t transportMessagesReceived{0};

    /** @brief Transport bytes received. */
    std::uint64_t transportBytesReceived{0};

    /** @brief Transport messages sent. */
    std::uint64_t transportMessagesSent{0};

    /** @brief Transport bytes sent. */
    std::uint64_t transportBytesSent{0};

    /** @brief Invalid protocol messages observed. */
    std::uint64_t protocolErrors{0};

    /** @brief Runtime errors recorded by components. */
    std::uint64_t errors{0};

    /** @brief Number of measured command executions. */
    std::uint64_t commandDurationCount{0};

    /** @brief Total measured command execution time in microseconds. */
    std::uint64_t commandDurationTotalMicros{0};

    /** @brief Longest measured command execution in microseconds. */
    std::uint64_t commandDurationMaxMicros{0};

    /** @brief Number of measured snapshot operations. */
    std::uint64_t snapshotDurationCount{0};

    /** @brief Total measured snapshot time in microseconds. */
    std::uint64_t snapshotDurationTotalMicros{0};

    /** @brief Longest measured snapshot operation in microseconds. */
    std::uint64_t snapshotDurationMaxMicros{0};

    /** @brief Number of measured replay operations. */
    std::uint64_t replayDurationCount{0};

    /** @brief Total measured replay time in microseconds. */
    std::uint64_t replayDurationTotalMicros{0};

    /** @brief Longest measured replay operation in microseconds. */
    std::uint64_t replayDurationMaxMicros{0};

    /**
     * @brief Return the average measured command duration.
     *
     * @return Average command duration, or zero when no command was measured.
     */
    [[nodiscard]] std::chrono::microseconds
    average_command_duration() const noexcept;

    /**
     * @brief Return the longest measured command duration.
     *
     * @return Maximum command duration.
     */
    [[nodiscard]] std::chrono::microseconds
    maximum_command_duration() const noexcept;

    /**
     * @brief Return the average measured snapshot duration.
     *
     * @return Average snapshot duration, or zero when none was measured.
     */
    [[nodiscard]] std::chrono::microseconds
    average_snapshot_duration() const noexcept;

    /**
     * @brief Return the longest measured snapshot duration.
     *
     * @return Maximum snapshot duration.
     */
    [[nodiscard]] std::chrono::microseconds
    maximum_snapshot_duration() const noexcept;

    /**
     * @brief Return the average measured replay duration.
     *
     * @return Average replay duration, or zero when none was measured.
     */
    [[nodiscard]] std::chrono::microseconds
    average_replay_duration() const noexcept;

    /**
     * @brief Return the longest measured replay duration.
     *
     * @return Maximum replay duration.
     */
    [[nodiscard]] std::chrono::microseconds
    maximum_replay_duration() const noexcept;

    /**
     * @brief Return the fraction of event deliveries that succeeded.
     *
     * @return Value between zero and one. Returns one when no delivery was
     *         attempted.
     */
    [[nodiscard]] double
    event_delivery_success_rate() const noexcept;

    /**
     * @brief Return the fraction of session resume attempts that succeeded.
     *
     * @return Value between zero and one. Returns one when no attempt exists.
     */
    [[nodiscard]] double
    resume_success_rate() const noexcept;

    /**
     * @brief Return whether any runtime error was recorded.
     *
     * @return True when runtime or protocol errors are non-zero.
     */
    [[nodiscard]] bool has_errors() const noexcept;
  };

  /**
   * @brief Thread-safe metrics collector for Realtime runtime components.
   *
   * Every update uses relaxed atomics. Metrics are observational and do not
   * participate in authoritative room state, command ordering, or persistence.
   *
   * Counter additions saturate at `std::uint64_t` maximum instead of wrapping.
   * Gauge decrements saturate at zero.
   */
  class VIX_REALTIME_API Metrics
  {
  public:
    /**
     * @brief Construct an empty metrics collector.
     */
    Metrics() = default;

    /**
     * @brief Capture a consistent-enough point-in-time metrics snapshot.
     *
     * Individual atomics may change while the snapshot is being assembled.
     * The returned values are suitable for observability and health reporting,
     * but do not represent a transactional runtime state.
     *
     * @return Current metrics snapshot.
     */
    [[nodiscard]] MetricsSnapshot snapshot() const noexcept;

    /**
     * @brief Reset every gauge and cumulative counter to zero.
     */
    void reset() noexcept;

    /**
     * @brief Replace the active room gauge.
     *
     * @param value Current active room count.
     */
    void set_active_rooms(
        std::size_t value) noexcept;

    /**
     * @brief Increment the active room gauge.
     *
     * @param count Number of rooms added.
     */
    void increment_active_rooms(
        std::size_t count = 1) noexcept;

    /**
     * @brief Decrement the active room gauge without underflow.
     *
     * @param count Number of rooms removed.
     */
    void decrement_active_rooms(
        std::size_t count = 1) noexcept;

    /**
     * @brief Replace the active session gauge.
     *
     * @param value Current logical session count.
     */
    void set_active_sessions(
        std::size_t value) noexcept;

    /**
     * @brief Increment the active session gauge.
     *
     * @param count Number of sessions added.
     */
    void increment_active_sessions(
        std::size_t count = 1) noexcept;

    /**
     * @brief Decrement the active session gauge without underflow.
     *
     * @param count Number of sessions removed.
     */
    void decrement_active_sessions(
        std::size_t count = 1) noexcept;

    /**
     * @brief Replace the attached connection gauge.
     *
     * @param value Current attached connection count.
     */
    void set_attached_connections(
        std::size_t value) noexcept;

    /**
     * @brief Increment the attached connection gauge.
     *
     * @param count Number of attached connections added.
     */
    void increment_attached_connections(
        std::size_t count = 1) noexcept;

    /**
     * @brief Decrement the attached connection gauge without underflow.
     *
     * @param count Number of connections removed.
     */
    void decrement_attached_connections(
        std::size_t count = 1) noexcept;

    /**
     * @brief Replace the queued command gauge.
     *
     * @param value Current queued command count.
     */
    void set_queued_commands(
        std::size_t value) noexcept;

    /**
     * @brief Increment the queued command gauge.
     *
     * @param count Number of queued commands added.
     */
    void increment_queued_commands(
        std::size_t count = 1) noexcept;

    /**
     * @brief Decrement the queued command gauge without underflow.
     *
     * @param count Number of commands removed.
     */
    void decrement_queued_commands(
        std::size_t count = 1) noexcept;

    /**
     * @brief Replace the active presence gauge.
     *
     * @param value Current active presence count.
     */
    void set_active_presence(
        std::size_t value) noexcept;

    /**
     * @brief Increment the active presence gauge.
     *
     * @param count Number of presence records added.
     */
    void increment_active_presence(
        std::size_t count = 1) noexcept;

    /**
     * @brief Decrement the active presence gauge without underflow.
     *
     * @param count Number of presence records removed.
     */
    void decrement_active_presence(
        std::size_t count = 1) noexcept;

    /**
     * @brief Record successful room opening.
     *
     * @param count Number of rooms opened.
     */
    void record_room_opened(
        std::size_t count = 1) noexcept;

    /**
     * @brief Record successful room closure.
     *
     * @param count Number of rooms closed.
     */
    void record_room_closed(
        std::size_t count = 1) noexcept;

    /**
     * @brief Record logical session creation.
     *
     * @param count Number of sessions created.
     */
    void record_session_created(
        std::size_t count = 1) noexcept;

    /**
     * @brief Record permanent logical session closure.
     *
     * @param count Number of sessions closed.
     */
    void record_session_closed(
        std::size_t count = 1) noexcept;

    /**
     * @brief Record transport connection attachment.
     *
     * @param count Number of connections attached.
     */
    void record_connection_attached(
        std::size_t count = 1) noexcept;

    /**
     * @brief Record transport connection detachment.
     *
     * @param count Number of connections detached.
     */
    void record_connection_detached(
        std::size_t count = 1) noexcept;

    /**
     * @brief Record commands successfully inserted into room queues.
     *
     * @param count Number of commands enqueued.
     */
    void record_command_enqueued(
        std::size_t count = 1) noexcept;

    /**
     * @brief Record one processed command result and execution duration.
     *
     * @param status Final command result status.
     * @param duration Measured command execution duration.
     */
    void record_command_result(
        CommandStatus status,
        std::chrono::microseconds duration = {}) noexcept;

    /**
     * @brief Record authoritative events persisted by an event store.
     *
     * @param count Number of persisted events.
     */
    void record_events_persisted(
        std::size_t count) noexcept;

    /**
     * @brief Record one event dispatch operation.
     *
     * @param recipients Number of selected recipients.
     * @param delivered Number of successful deliveries.
     * @param failed Number of failed deliveries.
     */
    void record_event_dispatch(
        std::size_t recipients,
        std::size_t delivered,
        std::size_t failed) noexcept;

    /**
     * @brief Record one successfully created snapshot.
     *
     * @param duration Measured snapshot persistence duration.
     */
    void record_snapshot_created(
        std::chrono::microseconds duration = {}) noexcept;

    /**
     * @brief Record one successfully restored snapshot.
     *
     * @param duration Measured snapshot restoration duration.
     */
    void record_snapshot_restored(
        std::chrono::microseconds duration = {}) noexcept;

    /**
     * @brief Record one successful replay operation.
     *
     * @param eventCount Number of replayed events.
     * @param byteCount Number of serialized replay bytes.
     * @param duration Measured replay duration.
     */
    void record_replay(
        std::size_t eventCount,
        std::size_t byteCount,
        std::chrono::microseconds duration = {}) noexcept;

    /**
     * @brief Record one session resumption attempt.
     *
     * @param succeeded Whether the attempt succeeded.
     */
    void record_resume_attempt(
        bool succeeded) noexcept;

    /**
     * @brief Record logical room presence creation.
     *
     * @param count Number of presence joins.
     */
    void record_presence_join(
        std::size_t count = 1) noexcept;

    /**
     * @brief Record logical room presence removal.
     *
     * @param count Number of presence leaves.
     */
    void record_presence_leave(
        std::size_t count = 1) noexcept;

    /**
     * @brief Record one received transport message.
     *
     * @param byteCount Raw message size.
     */
    void record_transport_received(
        std::size_t byteCount) noexcept;

    /**
     * @brief Record one sent transport message.
     *
     * @param byteCount Raw message size.
     */
    void record_transport_sent(
        std::size_t byteCount) noexcept;

    /**
     * @brief Record one invalid protocol message.
     *
     * @param count Number of protocol failures.
     */
    void record_protocol_error(
        std::size_t count = 1) noexcept;

    /**
     * @brief Record one runtime error.
     *
     * @param count Number of errors.
     */
    void record_error(
        std::size_t count = 1) noexcept;

  private:
    /** @brief Current active room gauge. */
    std::atomic<std::uint64_t> activeRooms_{0};

    /** @brief Current logical session gauge. */
    std::atomic<std::uint64_t> activeSessions_{0};

    /** @brief Current attached connection gauge. */
    std::atomic<std::uint64_t> attachedConnections_{0};

    /** @brief Current queued command gauge. */
    std::atomic<std::uint64_t> queuedCommands_{0};

    /** @brief Current active presence gauge. */
    std::atomic<std::uint64_t> activePresence_{0};

    /** @brief Successful room opening count. */
    std::atomic<std::uint64_t> roomsOpened_{0};

    /** @brief Successful room closure count. */
    std::atomic<std::uint64_t> roomsClosed_{0};

    /** @brief Logical session creation count. */
    std::atomic<std::uint64_t> sessionsCreated_{0};

    /** @brief Logical session closure count. */
    std::atomic<std::uint64_t> sessionsClosed_{0};

    /** @brief Connection attachment count. */
    std::atomic<std::uint64_t> connectionsAttached_{0};

    /** @brief Connection detachment count. */
    std::atomic<std::uint64_t> connectionsDetached_{0};

    /** @brief Successfully queued command count. */
    std::atomic<std::uint64_t> commandsEnqueued_{0};

    /** @brief Processed command count. */
    std::atomic<std::uint64_t> commandsProcessed_{0};

    /** @brief Accepted command count. */
    std::atomic<std::uint64_t> commandsAccepted_{0};

    /** @brief Rejected command count. */
    std::atomic<std::uint64_t> commandsRejected_{0};

    /** @brief Ignored command count. */
    std::atomic<std::uint64_t> commandsIgnored_{0};

    /** @brief Persisted event count. */
    std::atomic<std::uint64_t> eventsPersisted_{0};

    /** @brief Event dispatch operation count. */
    std::atomic<std::uint64_t> eventDispatches_{0};

    /** @brief Total selected event recipient count. */
    std::atomic<std::uint64_t> eventRecipients_{0};

    /** @brief Successful event delivery count. */
    std::atomic<std::uint64_t> eventDeliveriesSucceeded_{0};

    /** @brief Failed event delivery count. */
    std::atomic<std::uint64_t> eventDeliveriesFailed_{0};

    /** @brief Successfully persisted snapshot count. */
    std::atomic<std::uint64_t> snapshotsCreated_{0};

    /** @brief Successfully restored snapshot count. */
    std::atomic<std::uint64_t> snapshotsRestored_{0};

    /** @brief Successful replay operation count. */
    std::atomic<std::uint64_t> replayOperations_{0};

    /** @brief Replayed event count. */
    std::atomic<std::uint64_t> replayEventsApplied_{0};

    /** @brief Replayed serialized byte count. */
    std::atomic<std::uint64_t> replayBytes_{0};

    /** @brief Session resume attempt count. */
    std::atomic<std::uint64_t> resumeAttempts_{0};

    /** @brief Successful session resume count. */
    std::atomic<std::uint64_t> resumeSucceeded_{0};

    /** @brief Failed session resume count. */
    std::atomic<std::uint64_t> resumeFailed_{0};

    /** @brief Logical presence join count. */
    std::atomic<std::uint64_t> presenceJoins_{0};

    /** @brief Logical presence leave count. */
    std::atomic<std::uint64_t> presenceLeaves_{0};

    /** @brief Received transport message count. */
    std::atomic<std::uint64_t> transportMessagesReceived_{0};

    /** @brief Received transport byte count. */
    std::atomic<std::uint64_t> transportBytesReceived_{0};

    /** @brief Sent transport message count. */
    std::atomic<std::uint64_t> transportMessagesSent_{0};

    /** @brief Sent transport byte count. */
    std::atomic<std::uint64_t> transportBytesSent_{0};

    /** @brief Invalid protocol message count. */
    std::atomic<std::uint64_t> protocolErrors_{0};

    /** @brief Runtime error count. */
    std::atomic<std::uint64_t> errors_{0};

    /** @brief Number of measured command durations. */
    std::atomic<std::uint64_t> commandDurationCount_{0};

    /** @brief Total command duration in microseconds. */
    std::atomic<std::uint64_t> commandDurationTotalMicros_{0};

    /** @brief Maximum command duration in microseconds. */
    std::atomic<std::uint64_t> commandDurationMaxMicros_{0};

    /** @brief Number of measured snapshot durations. */
    std::atomic<std::uint64_t> snapshotDurationCount_{0};

    /** @brief Total snapshot duration in microseconds. */
    std::atomic<std::uint64_t> snapshotDurationTotalMicros_{0};

    /** @brief Maximum snapshot duration in microseconds. */
    std::atomic<std::uint64_t> snapshotDurationMaxMicros_{0};

    /** @brief Number of measured replay durations. */
    std::atomic<std::uint64_t> replayDurationCount_{0};

    /** @brief Total replay duration in microseconds. */
    std::atomic<std::uint64_t> replayDurationTotalMicros_{0};

    /** @brief Maximum replay duration in microseconds. */
    std::atomic<std::uint64_t> replayDurationMaxMicros_{0};
  };

  /**
   * @brief Shared ownership pointer for a metrics collector.
   */
  using MetricsPtr = std::shared_ptr<Metrics>;

} // namespace vix::realtime

#endif // VIX_REALTIME_METRICS_HPP
