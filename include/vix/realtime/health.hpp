/**
 *
 * @file health.hpp
 * @author Gaspard Kirira
 * @brief Runtime health inspection for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_HEALTH_HPP
#define VIX_REALTIME_HEALTH_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/metrics.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/server.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Overall health state of a Realtime runtime.
   */
  enum class HealthStatus : std::uint8_t
  {
    /** @brief The runtime is operating normally. */
    Healthy = 0,

    /** @brief The runtime remains operational but requires attention. */
    Degraded,

    /** @brief The runtime cannot safely provide normal service. */
    Unhealthy,

    /** @brief The runtime is not currently running. */
    Stopped
  };

  /**
   * @brief Return the stable textual representation of a health status.
   *
   * @param status Health status.
   * @return Stable lowercase identifier.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(HealthStatus status) noexcept
  {
    switch (status)
    {
    case HealthStatus::Healthy:
      return "healthy";

    case HealthStatus::Degraded:
      return "degraded";

    case HealthStatus::Unhealthy:
      return "unhealthy";

    case HealthStatus::Stopped:
      return "stopped";
    }

    return "unhealthy";
  }

  /**
   * @brief Configuration controlling runtime health evaluation.
   */
  struct HealthOptions
  {
    /**
     * @brief Require a snapshot store for healthy operation.
     *
     * When enabled, a missing snapshot store marks the runtime unhealthy.
     */
    bool requireSnapshotStore{false};

    /**
     * @brief Require a presence store when presence is enabled.
     */
    bool requirePresenceStoreWhenEnabled{true};

    /**
     * @brief Mark detached sessions as a degraded condition.
     */
    bool degradeOnDetachedSessions{false};

    /**
     * @brief Maximum total queued commands before degradation.
     *
     * Zero disables this threshold.
     */
    std::size_t maxQueuedCommands{0};

    /**
     * @brief Number of cumulative runtime errors tolerated before degradation.
     */
    std::uint64_t recordedErrorTolerance{0};

    /**
     * @brief Number of cumulative protocol errors tolerated before degradation.
     */
    std::uint64_t protocolErrorTolerance{0};
  };

  /**
   * @brief Point-in-time health report for a Realtime runtime.
   */
  struct HealthReport
  {
    /** @brief Overall evaluated runtime health. */
    HealthStatus status{HealthStatus::Stopped};

    /** @brief Time at which the report was created. */
    Timestamp checkedAt{SystemClock::now()};

    /** @brief Local runtime node identifier. */
    NodeId nodeId{};

    /** @brief Current server lifecycle status. */
    ServerStatus serverStatus{ServerStatus::Created};

    /** @brief Number of rooms currently registered in the manager. */
    std::size_t roomCount{0};

    /** @brief Number of rooms currently open. */
    std::size_t openRoomCount{0};

    /** @brief Number of rooms currently changing lifecycle state. */
    std::size_t transitioningRoomCount{0};

    /** @brief Number of closed rooms retained by the manager. */
    std::size_t closedRoomCount{0};

    /** @brief Number of rooms in an unrecoverable failed state. */
    std::size_t failedRoomCount{0};

    /** @brief Number of open rooms owned by the local node. */
    std::size_t locallyOwnedRoomCount{0};

    /** @brief Total commands currently waiting in room queues. */
    std::size_t queuedCommandCount{0};

    /** @brief Number of logical sessions registered in the manager. */
    std::size_t sessionCount{0};

    /** @brief Number of sessions with active connections. */
    std::size_t connectedSessionCount{0};

    /** @brief Number of temporarily detached sessions. */
    std::size_t detachedSessionCount{0};

    /** @brief Number of closed sessions retained by the manager. */
    std::size_t closedSessionCount{0};

    /** @brief Number of records currently stored by the presence store. */
    std::size_t presenceCount{0};

    /** @brief Whether the authoritative event store is configured. */
    bool eventStoreAvailable{false};

    /** @brief Whether a snapshot store is configured. */
    bool snapshotStoreAvailable{false};

    /** @brief Whether logical presence is enabled. */
    bool presenceEnabled{false};

    /** @brief Whether a presence store is configured. */
    bool presenceStoreAvailable{false};

    /** @brief Whether a room ownership directory is configured. */
    bool roomDirectoryAvailable{false};

    /** @brief Whether a metrics collector was available. */
    bool metricsAvailable{false};

    /** @brief Captured runtime metrics. */
    MetricsSnapshot metrics{};

    /** @brief Human-readable health findings. */
    std::vector<std::string> issues{};

    /**
     * @brief Return whether the runtime is completely healthy.
     *
     * @return True when status is `Healthy`.
     */
    [[nodiscard]] bool healthy() const noexcept
    {
      return status == HealthStatus::Healthy;
    }

    /**
     * @brief Return whether the runtime may still serve requests.
     *
     * @return True for healthy and degraded runtimes.
     */
    [[nodiscard]] bool operational() const noexcept
    {
      return status == HealthStatus::Healthy ||
             status == HealthStatus::Degraded;
    }

    /**
     * @brief Return whether health findings were recorded.
     *
     * @return True when at least one issue exists.
     */
    [[nodiscard]] bool has_issues() const noexcept
    {
      return !issues.empty();
    }
  };

  /**
   * @brief Inspects the health of a Realtime server and its runtime manager.
   *
   * Health inspection is observational. It does not mutate rooms, sessions,
   * ownership, presence, or metrics.
   */
  class VIX_REALTIME_API HealthMonitor
  {
  public:
    /**
     * @brief Construct a runtime health monitor.
     *
     * @param server Realtime server to inspect.
     * @param metrics Optional runtime metrics collector.
     * @param options Health evaluation configuration.
     *
     * @throws vix::realtime::Error
     *         When the server is null.
     */
    explicit HealthMonitor(
        ServerPtr server,
        MetricsPtr metrics = nullptr,
        HealthOptions options = {});

    /**
     * @brief Inspect the current Realtime runtime health.
     *
     * @return Point-in-time health report.
     */
    [[nodiscard]] HealthReport check() const;

    /**
     * @brief Return the monitored Realtime server.
     *
     * @return Shared server.
     */
    [[nodiscard]] const ServerPtr &
    server() const noexcept;

    /**
     * @brief Return the optional metrics collector.
     *
     * @return Shared metrics collector, or null.
     */
    [[nodiscard]] const MetricsPtr &
    metrics() const noexcept;

    /**
     * @brief Return the health evaluation configuration.
     *
     * @return Health options.
     */
    [[nodiscard]] const HealthOptions &
    options() const noexcept;

  private:
    /**
     * @brief Raise the report status to at least degraded.
     */
    static void mark_degraded(
        HealthReport &report,
        std::string issue);

    /**
     * @brief Raise the report status to unhealthy.
     */
    static void mark_unhealthy(
        HealthReport &report,
        std::string issue);

    /** @brief Realtime server being inspected. */
    ServerPtr server_{};

    /** @brief Optional runtime metrics collector. */
    MetricsPtr metrics_{};

    /** @brief Health evaluation configuration. */
    HealthOptions options_{};
  };

  /**
   * @brief Shared ownership pointer for a health monitor.
   */
  using HealthMonitorPtr =
      std::shared_ptr<HealthMonitor>;

} // namespace vix::realtime

#endif // VIX_REALTIME_HEALTH_HPP
