/**
 *
 * @file metrics.cpp
 * @author Gaspard Kirira
 * @brief Implementation of thread-safe Vix Realtime runtime metrics.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/metrics.hpp>

#include <limits>

namespace vix::realtime
{
  namespace
  {
    /**
     * @brief Convert a size value to a saturated 64-bit counter value.
     */
    [[nodiscard]] std::uint64_t to_counter(
        std::size_t value) noexcept
    {
      if constexpr (
          sizeof(std::size_t) >
          sizeof(std::uint64_t))
      {
        if (value >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint64_t>::max()))
        {
          return std::numeric_limits<std::uint64_t>::max();
        }
      }

      return static_cast<std::uint64_t>(value);
    }

    /**
     * @brief Add to an atomic counter without unsigned overflow.
     */
    void saturating_add(
        std::atomic<std::uint64_t> &target,
        std::uint64_t value) noexcept
    {
      std::uint64_t current =
          target.load(
              std::memory_order_relaxed);

      for (;;)
      {
        const std::uint64_t maximum =
            std::numeric_limits<std::uint64_t>::max();

        const std::uint64_t next =
            value > maximum - current
                ? maximum
                : current + value;

        if (target.compare_exchange_weak(
                current,
                next,
                std::memory_order_relaxed,
                std::memory_order_relaxed))
        {
          return;
        }
      }
    }

    /**
     * @brief Subtract from an atomic gauge without unsigned underflow.
     */
    void saturating_subtract(
        std::atomic<std::uint64_t> &target,
        std::uint64_t value) noexcept
    {
      std::uint64_t current =
          target.load(
              std::memory_order_relaxed);

      for (;;)
      {
        const std::uint64_t next =
            value >= current
                ? 0
                : current - value;

        if (target.compare_exchange_weak(
                current,
                next,
                std::memory_order_relaxed,
                std::memory_order_relaxed))
        {
          return;
        }
      }
    }

    /**
     * @brief Update an atomic maximum value.
     */
    void update_maximum(
        std::atomic<std::uint64_t> &target,
        std::uint64_t value) noexcept
    {
      std::uint64_t current =
          target.load(
              std::memory_order_relaxed);

      while (current < value &&
             !target.compare_exchange_weak(
                 current,
                 value,
                 std::memory_order_relaxed,
                 std::memory_order_relaxed))
      {
      }
    }

    /**
     * @brief Convert a duration to non-negative microseconds.
     */
    [[nodiscard]] std::uint64_t duration_micros(
        std::chrono::microseconds duration) noexcept
    {
      if (duration.count() <= 0)
      {
        return 0;
      }

      using DurationRep =
          std::chrono::microseconds::rep;

      if constexpr (
          sizeof(DurationRep) >
          sizeof(std::uint64_t))
      {
        if (duration.count() >
            static_cast<DurationRep>(
                std::numeric_limits<std::uint64_t>::max()))
        {
          return std::numeric_limits<std::uint64_t>::max();
        }
      }

      return static_cast<std::uint64_t>(
          duration.count());
    }

    /**
     * @brief Record one measured operation duration.
     */
    void record_duration(
        std::atomic<std::uint64_t> &count,
        std::atomic<std::uint64_t> &total,
        std::atomic<std::uint64_t> &maximum,
        std::chrono::microseconds duration) noexcept
    {
      const std::uint64_t value =
          duration_micros(duration);

      saturating_add(count, 1);
      saturating_add(total, value);
      update_maximum(maximum, value);
    }

    /**
     * @brief Return an average duration from aggregate values.
     */
    [[nodiscard]] std::chrono::microseconds
    average_duration(
        std::uint64_t count,
        std::uint64_t total) noexcept
    {
      if (count == 0)
      {
        return {};
      }

      const std::uint64_t average =
          total / count;

      using DurationRep =
          std::chrono::microseconds::rep;

      const std::uint64_t maximum =
          static_cast<std::uint64_t>(
              std::numeric_limits<DurationRep>::max());

      return std::chrono::microseconds{
          static_cast<DurationRep>(
              average > maximum
                  ? maximum
                  : average)};
    }

    /**
     * @brief Convert a counter duration to std::chrono microseconds.
     */
    [[nodiscard]] std::chrono::microseconds
    maximum_duration(
        std::uint64_t value) noexcept
    {
      using DurationRep =
          std::chrono::microseconds::rep;

      const std::uint64_t maximum =
          static_cast<std::uint64_t>(
              std::numeric_limits<DurationRep>::max());

      return std::chrono::microseconds{
          static_cast<DurationRep>(
              value > maximum
                  ? maximum
                  : value)};
    }

  } // namespace

  std::chrono::microseconds
  MetricsSnapshot::average_command_duration() const noexcept
  {
    return average_duration(
        commandDurationCount,
        commandDurationTotalMicros);
  }

  std::chrono::microseconds
  MetricsSnapshot::maximum_command_duration() const noexcept
  {
    return maximum_duration(
        commandDurationMaxMicros);
  }

  std::chrono::microseconds
  MetricsSnapshot::average_snapshot_duration() const noexcept
  {
    return average_duration(
        snapshotDurationCount,
        snapshotDurationTotalMicros);
  }

  std::chrono::microseconds
  MetricsSnapshot::maximum_snapshot_duration() const noexcept
  {
    return maximum_duration(
        snapshotDurationMaxMicros);
  }

  std::chrono::microseconds
  MetricsSnapshot::average_replay_duration() const noexcept
  {
    return average_duration(
        replayDurationCount,
        replayDurationTotalMicros);
  }

  std::chrono::microseconds
  MetricsSnapshot::maximum_replay_duration() const noexcept
  {
    return maximum_duration(
        replayDurationMaxMicros);
  }

  double
  MetricsSnapshot::event_delivery_success_rate() const noexcept
  {
    const std::uint64_t total =
        eventDeliveriesSucceeded +
        eventDeliveriesFailed;

    if (total == 0)
    {
      return 1.0;
    }

    return static_cast<double>(
               eventDeliveriesSucceeded) /
           static_cast<double>(total);
  }

  double
  MetricsSnapshot::resume_success_rate() const noexcept
  {
    if (resumeAttempts == 0)
    {
      return 1.0;
    }

    return static_cast<double>(
               resumeSucceeded) /
           static_cast<double>(
               resumeAttempts);
  }

  bool MetricsSnapshot::has_errors() const noexcept
  {
    return errors != 0 ||
           protocolErrors != 0;
  }

  MetricsSnapshot Metrics::snapshot() const noexcept
  {
    MetricsSnapshot result;

    result.activeRooms =
        activeRooms_.load(
            std::memory_order_relaxed);

    result.activeSessions =
        activeSessions_.load(
            std::memory_order_relaxed);

    result.attachedConnections =
        attachedConnections_.load(
            std::memory_order_relaxed);

    result.queuedCommands =
        queuedCommands_.load(
            std::memory_order_relaxed);

    result.activePresence =
        activePresence_.load(
            std::memory_order_relaxed);

    result.roomsOpened =
        roomsOpened_.load(
            std::memory_order_relaxed);

    result.roomsClosed =
        roomsClosed_.load(
            std::memory_order_relaxed);

    result.sessionsCreated =
        sessionsCreated_.load(
            std::memory_order_relaxed);

    result.sessionsClosed =
        sessionsClosed_.load(
            std::memory_order_relaxed);

    result.connectionsAttached =
        connectionsAttached_.load(
            std::memory_order_relaxed);

    result.connectionsDetached =
        connectionsDetached_.load(
            std::memory_order_relaxed);

    result.commandsEnqueued =
        commandsEnqueued_.load(
            std::memory_order_relaxed);

    result.commandsProcessed =
        commandsProcessed_.load(
            std::memory_order_relaxed);

    result.commandsAccepted =
        commandsAccepted_.load(
            std::memory_order_relaxed);

    result.commandsRejected =
        commandsRejected_.load(
            std::memory_order_relaxed);

    result.commandsIgnored =
        commandsIgnored_.load(
            std::memory_order_relaxed);

    result.eventsPersisted =
        eventsPersisted_.load(
            std::memory_order_relaxed);

    result.eventDispatches =
        eventDispatches_.load(
            std::memory_order_relaxed);

    result.eventRecipients =
        eventRecipients_.load(
            std::memory_order_relaxed);

    result.eventDeliveriesSucceeded =
        eventDeliveriesSucceeded_.load(
            std::memory_order_relaxed);

    result.eventDeliveriesFailed =
        eventDeliveriesFailed_.load(
            std::memory_order_relaxed);

    result.snapshotsCreated =
        snapshotsCreated_.load(
            std::memory_order_relaxed);

    result.snapshotsRestored =
        snapshotsRestored_.load(
            std::memory_order_relaxed);

    result.replayOperations =
        replayOperations_.load(
            std::memory_order_relaxed);

    result.replayEventsApplied =
        replayEventsApplied_.load(
            std::memory_order_relaxed);

    result.replayBytes =
        replayBytes_.load(
            std::memory_order_relaxed);

    result.resumeAttempts =
        resumeAttempts_.load(
            std::memory_order_relaxed);

    result.resumeSucceeded =
        resumeSucceeded_.load(
            std::memory_order_relaxed);

    result.resumeFailed =
        resumeFailed_.load(
            std::memory_order_relaxed);

    result.presenceJoins =
        presenceJoins_.load(
            std::memory_order_relaxed);

    result.presenceLeaves =
        presenceLeaves_.load(
            std::memory_order_relaxed);

    result.transportMessagesReceived =
        transportMessagesReceived_.load(
            std::memory_order_relaxed);

    result.transportBytesReceived =
        transportBytesReceived_.load(
            std::memory_order_relaxed);

    result.transportMessagesSent =
        transportMessagesSent_.load(
            std::memory_order_relaxed);

    result.transportBytesSent =
        transportBytesSent_.load(
            std::memory_order_relaxed);

    result.protocolErrors =
        protocolErrors_.load(
            std::memory_order_relaxed);

    result.errors =
        errors_.load(
            std::memory_order_relaxed);

    result.commandDurationCount =
        commandDurationCount_.load(
            std::memory_order_relaxed);

    result.commandDurationTotalMicros =
        commandDurationTotalMicros_.load(
            std::memory_order_relaxed);

    result.commandDurationMaxMicros =
        commandDurationMaxMicros_.load(
            std::memory_order_relaxed);

    result.snapshotDurationCount =
        snapshotDurationCount_.load(
            std::memory_order_relaxed);

    result.snapshotDurationTotalMicros =
        snapshotDurationTotalMicros_.load(
            std::memory_order_relaxed);

    result.snapshotDurationMaxMicros =
        snapshotDurationMaxMicros_.load(
            std::memory_order_relaxed);

    result.replayDurationCount =
        replayDurationCount_.load(
            std::memory_order_relaxed);

    result.replayDurationTotalMicros =
        replayDurationTotalMicros_.load(
            std::memory_order_relaxed);

    result.replayDurationMaxMicros =
        replayDurationMaxMicros_.load(
            std::memory_order_relaxed);

    return result;
  }

  void Metrics::reset() noexcept
  {
    activeRooms_.store(0, std::memory_order_relaxed);
    activeSessions_.store(0, std::memory_order_relaxed);
    attachedConnections_.store(0, std::memory_order_relaxed);
    queuedCommands_.store(0, std::memory_order_relaxed);
    activePresence_.store(0, std::memory_order_relaxed);

    roomsOpened_.store(0, std::memory_order_relaxed);
    roomsClosed_.store(0, std::memory_order_relaxed);
    sessionsCreated_.store(0, std::memory_order_relaxed);
    sessionsClosed_.store(0, std::memory_order_relaxed);
    connectionsAttached_.store(0, std::memory_order_relaxed);
    connectionsDetached_.store(0, std::memory_order_relaxed);

    commandsEnqueued_.store(0, std::memory_order_relaxed);
    commandsProcessed_.store(0, std::memory_order_relaxed);
    commandsAccepted_.store(0, std::memory_order_relaxed);
    commandsRejected_.store(0, std::memory_order_relaxed);
    commandsIgnored_.store(0, std::memory_order_relaxed);

    eventsPersisted_.store(0, std::memory_order_relaxed);
    eventDispatches_.store(0, std::memory_order_relaxed);
    eventRecipients_.store(0, std::memory_order_relaxed);
    eventDeliveriesSucceeded_.store(0, std::memory_order_relaxed);
    eventDeliveriesFailed_.store(0, std::memory_order_relaxed);

    snapshotsCreated_.store(0, std::memory_order_relaxed);
    snapshotsRestored_.store(0, std::memory_order_relaxed);
    replayOperations_.store(0, std::memory_order_relaxed);
    replayEventsApplied_.store(0, std::memory_order_relaxed);
    replayBytes_.store(0, std::memory_order_relaxed);

    resumeAttempts_.store(0, std::memory_order_relaxed);
    resumeSucceeded_.store(0, std::memory_order_relaxed);
    resumeFailed_.store(0, std::memory_order_relaxed);

    presenceJoins_.store(0, std::memory_order_relaxed);
    presenceLeaves_.store(0, std::memory_order_relaxed);

    transportMessagesReceived_.store(0, std::memory_order_relaxed);
    transportBytesReceived_.store(0, std::memory_order_relaxed);
    transportMessagesSent_.store(0, std::memory_order_relaxed);
    transportBytesSent_.store(0, std::memory_order_relaxed);

    protocolErrors_.store(0, std::memory_order_relaxed);
    errors_.store(0, std::memory_order_relaxed);

    commandDurationCount_.store(0, std::memory_order_relaxed);
    commandDurationTotalMicros_.store(0, std::memory_order_relaxed);
    commandDurationMaxMicros_.store(0, std::memory_order_relaxed);

    snapshotDurationCount_.store(0, std::memory_order_relaxed);
    snapshotDurationTotalMicros_.store(0, std::memory_order_relaxed);
    snapshotDurationMaxMicros_.store(0, std::memory_order_relaxed);

    replayDurationCount_.store(0, std::memory_order_relaxed);
    replayDurationTotalMicros_.store(0, std::memory_order_relaxed);
    replayDurationMaxMicros_.store(0, std::memory_order_relaxed);
  }

  void Metrics::set_active_rooms(
      std::size_t value) noexcept
  {
    activeRooms_.store(
        to_counter(value),
        std::memory_order_relaxed);
  }

  void Metrics::increment_active_rooms(
      std::size_t count) noexcept
  {
    saturating_add(
        activeRooms_,
        to_counter(count));
  }

  void Metrics::decrement_active_rooms(
      std::size_t count) noexcept
  {
    saturating_subtract(
        activeRooms_,
        to_counter(count));
  }

  void Metrics::set_active_sessions(
      std::size_t value) noexcept
  {
    activeSessions_.store(
        to_counter(value),
        std::memory_order_relaxed);
  }

  void Metrics::increment_active_sessions(
      std::size_t count) noexcept
  {
    saturating_add(
        activeSessions_,
        to_counter(count));
  }

  void Metrics::decrement_active_sessions(
      std::size_t count) noexcept
  {
    saturating_subtract(
        activeSessions_,
        to_counter(count));
  }

  void Metrics::set_attached_connections(
      std::size_t value) noexcept
  {
    attachedConnections_.store(
        to_counter(value),
        std::memory_order_relaxed);
  }

  void Metrics::increment_attached_connections(
      std::size_t count) noexcept
  {
    saturating_add(
        attachedConnections_,
        to_counter(count));
  }

  void Metrics::decrement_attached_connections(
      std::size_t count) noexcept
  {
    saturating_subtract(
        attachedConnections_,
        to_counter(count));
  }

  void Metrics::set_queued_commands(
      std::size_t value) noexcept
  {
    queuedCommands_.store(
        to_counter(value),
        std::memory_order_relaxed);
  }

  void Metrics::increment_queued_commands(
      std::size_t count) noexcept
  {
    saturating_add(
        queuedCommands_,
        to_counter(count));
  }

  void Metrics::decrement_queued_commands(
      std::size_t count) noexcept
  {
    saturating_subtract(
        queuedCommands_,
        to_counter(count));
  }

  void Metrics::set_active_presence(
      std::size_t value) noexcept
  {
    activePresence_.store(
        to_counter(value),
        std::memory_order_relaxed);
  }

  void Metrics::increment_active_presence(
      std::size_t count) noexcept
  {
    saturating_add(
        activePresence_,
        to_counter(count));
  }

  void Metrics::decrement_active_presence(
      std::size_t count) noexcept
  {
    saturating_subtract(
        activePresence_,
        to_counter(count));
  }

  void Metrics::record_room_opened(
      std::size_t count) noexcept
  {
    saturating_add(
        roomsOpened_,
        to_counter(count));
  }

  void Metrics::record_room_closed(
      std::size_t count) noexcept
  {
    saturating_add(
        roomsClosed_,
        to_counter(count));
  }

  void Metrics::record_session_created(
      std::size_t count) noexcept
  {
    saturating_add(
        sessionsCreated_,
        to_counter(count));
  }

  void Metrics::record_session_closed(
      std::size_t count) noexcept
  {
    saturating_add(
        sessionsClosed_,
        to_counter(count));
  }

  void Metrics::record_connection_attached(
      std::size_t count) noexcept
  {
    saturating_add(
        connectionsAttached_,
        to_counter(count));
  }

  void Metrics::record_connection_detached(
      std::size_t count) noexcept
  {
    saturating_add(
        connectionsDetached_,
        to_counter(count));
  }

  void Metrics::record_command_enqueued(
      std::size_t count) noexcept
  {
    saturating_add(
        commandsEnqueued_,
        to_counter(count));
  }

  void Metrics::record_command_result(
      CommandStatus status,
      std::chrono::microseconds duration) noexcept
  {
    saturating_add(
        commandsProcessed_,
        1);

    switch (status)
    {
    case CommandStatus::Accepted:
      saturating_add(
          commandsAccepted_,
          1);
      break;

    case CommandStatus::Rejected:
      saturating_add(
          commandsRejected_,
          1);
      break;

    case CommandStatus::Ignored:
      saturating_add(
          commandsIgnored_,
          1);
      break;
    }

    record_duration(
        commandDurationCount_,
        commandDurationTotalMicros_,
        commandDurationMaxMicros_,
        duration);
  }

  void Metrics::record_events_persisted(
      std::size_t count) noexcept
  {
    saturating_add(
        eventsPersisted_,
        to_counter(count));
  }

  void Metrics::record_event_dispatch(
      std::size_t recipients,
      std::size_t delivered,
      std::size_t failed) noexcept
  {
    saturating_add(
        eventDispatches_,
        1);

    const std::uint64_t deliveredValue =
        to_counter(delivered);

    const std::uint64_t failedValue =
        to_counter(failed);

    std::uint64_t selectedValue =
        to_counter(recipients);

    const std::uint64_t maximum =
        std::numeric_limits<std::uint64_t>::max();

    const std::uint64_t completedValue =
        failedValue > maximum - deliveredValue
            ? maximum
            : deliveredValue + failedValue;

    if (selectedValue < completedValue)
    {
      selectedValue = completedValue;
    }

    saturating_add(
        eventRecipients_,
        selectedValue);

    saturating_add(
        eventDeliveriesSucceeded_,
        deliveredValue);

    saturating_add(
        eventDeliveriesFailed_,
        failedValue);
  }

  void Metrics::record_snapshot_created(
      std::chrono::microseconds duration) noexcept
  {
    saturating_add(
        snapshotsCreated_,
        1);

    record_duration(
        snapshotDurationCount_,
        snapshotDurationTotalMicros_,
        snapshotDurationMaxMicros_,
        duration);
  }

  void Metrics::record_snapshot_restored(
      std::chrono::microseconds duration) noexcept
  {
    saturating_add(
        snapshotsRestored_,
        1);

    record_duration(
        snapshotDurationCount_,
        snapshotDurationTotalMicros_,
        snapshotDurationMaxMicros_,
        duration);
  }

  void Metrics::record_replay(
      std::size_t eventCount,
      std::size_t byteCount,
      std::chrono::microseconds duration) noexcept
  {
    saturating_add(
        replayOperations_,
        1);

    saturating_add(
        replayEventsApplied_,
        to_counter(eventCount));

    saturating_add(
        replayBytes_,
        to_counter(byteCount));

    record_duration(
        replayDurationCount_,
        replayDurationTotalMicros_,
        replayDurationMaxMicros_,
        duration);
  }

  void Metrics::record_resume_attempt(
      bool succeeded) noexcept
  {
    saturating_add(
        resumeAttempts_,
        1);

    if (succeeded)
    {
      saturating_add(
          resumeSucceeded_,
          1);
    }
    else
    {
      saturating_add(
          resumeFailed_,
          1);
    }
  }

  void Metrics::record_presence_join(
      std::size_t count) noexcept
  {
    saturating_add(
        presenceJoins_,
        to_counter(count));
  }

  void Metrics::record_presence_leave(
      std::size_t count) noexcept
  {
    saturating_add(
        presenceLeaves_,
        to_counter(count));
  }

  void Metrics::record_transport_received(
      std::size_t byteCount) noexcept
  {
    saturating_add(
        transportMessagesReceived_,
        1);

    saturating_add(
        transportBytesReceived_,
        to_counter(byteCount));
  }

  void Metrics::record_transport_sent(
      std::size_t byteCount) noexcept
  {
    saturating_add(
        transportMessagesSent_,
        1);

    saturating_add(
        transportBytesSent_,
        to_counter(byteCount));
  }

  void Metrics::record_protocol_error(
      std::size_t count) noexcept
  {
    saturating_add(
        protocolErrors_,
        to_counter(count));
  }

  void Metrics::record_error(
      std::size_t count) noexcept
  {
    saturating_add(
        errors_,
        to_counter(count));
  }

} // namespace vix::realtime
