/**
 *
 * @file health.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime runtime health inspection.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/health.hpp>

#include <limits>
#include <string>
#include <utility>
#include <exception>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/presence_store.hpp>
#include <vix/realtime/room.hpp>
#include <vix/realtime/room_directory.hpp>
#include <vix/realtime/room_manager.hpp>
#include <vix/realtime/session.hpp>

namespace vix::realtime
{
  namespace
  {
    /**
     * @brief Add two size values without overflowing.
     */
    [[nodiscard]] std::size_t saturating_add(
        std::size_t left,
        std::size_t right) noexcept
    {
      const std::size_t maximum =
          std::numeric_limits<std::size_t>::max();

      if (right > maximum - left)
      {
        return maximum;
      }

      return left + right;
    }

  } // namespace

  HealthMonitor::HealthMonitor(
      ServerPtr server,
      MetricsPtr metrics,
      HealthOptions options)
      : server_(std::move(server)),
        metrics_(std::move(metrics)),
        options_(std::move(options))
  {
    if (!server_)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "realtime health monitor requires a server"};
    }
  }

  HealthReport HealthMonitor::check() const
  {
    HealthReport report;

    report.checkedAt =
        SystemClock::now();

    report.nodeId =
        server_->node_id();

    report.serverStatus =
        server_->status();

    switch (report.serverStatus)
    {
    case ServerStatus::Running:
      report.status =
          HealthStatus::Healthy;
      break;

    case ServerStatus::Created:
      report.status =
          HealthStatus::Stopped;

      report.issues.emplace_back(
          "realtime server has not started");
      break;

    case ServerStatus::Stopping:
      report.status =
          HealthStatus::Degraded;

      report.issues.emplace_back(
          "realtime server is stopping");
      break;

    case ServerStatus::Stopped:
      report.status =
          HealthStatus::Stopped;

      report.issues.emplace_back(
          "realtime server is stopped");
      break;

    case ServerStatus::Failed:
      report.status =
          HealthStatus::Unhealthy;

      report.issues.emplace_back(
          "realtime server is in a failed state");
      break;
    }

    const RoomManagerPtr &manager =
        server_->manager();

    if (!manager)
    {
      mark_unhealthy(
          report,
          "realtime server has no room manager");

      return report;
    }

    report.eventStoreAvailable =
        manager->event_store() != nullptr;

    report.snapshotStoreAvailable =
        manager->snapshot_store() != nullptr;

    report.presenceEnabled =
        manager->config().enablePresence;

    report.presenceStoreAvailable =
        manager->presence_store() != nullptr;

    report.roomDirectoryAvailable =
        manager->room_directory() != nullptr;

    if (!report.eventStoreAvailable)
    {
      mark_unhealthy(
          report,
          "authoritative event store is unavailable");
    }

    if (!report.roomDirectoryAvailable)
    {
      mark_unhealthy(
          report,
          "room ownership directory is unavailable");
    }

    if (options_.requireSnapshotStore &&
        !report.snapshotStoreAvailable)
    {
      mark_unhealthy(
          report,
          "required snapshot store is unavailable");
    }

    if (options_.requirePresenceStoreWhenEnabled &&
        report.presenceEnabled &&
        !report.presenceStoreAvailable)
    {
      mark_unhealthy(
          report,
          "presence is enabled but its store is unavailable");
    }

    const std::vector<RoomId> roomIds =
        manager->room_ids();

    report.roomCount =
        roomIds.size();

    for (const auto &roomId : roomIds)
    {
      RoomPtr room =
          manager->find_room(roomId);

      if (!room)
      {
        mark_unhealthy(
            report,
            "room registry changed during health inspection");

        continue;
      }

      report.queuedCommandCount =
          saturating_add(
              report.queuedCommandCount,
              room->pending_command_count());

      switch (room->status())
      {
      case RoomStatus::Created:
      case RoomStatus::Opening:
      case RoomStatus::Closing:
        ++report.transitioningRoomCount;
        break;

      case RoomStatus::Open:
      {
        ++report.openRoomCount;

        if (report.roomDirectoryAvailable &&
            manager->room_directory()->owns(
                roomId,
                report.nodeId,
                report.checkedAt))
        {
          ++report.locallyOwnedRoomCount;
        }
        else
        {
          mark_unhealthy(
              report,
              std::string{
                  "open room is not owned by the local node: "} +
                  roomId.value());
        }

        break;
      }

      case RoomStatus::Closed:
        ++report.closedRoomCount;
        break;

      case RoomStatus::Failed:
        ++report.failedRoomCount;
        break;
      }
    }

    if (report.failedRoomCount != 0)
    {
      mark_unhealthy(
          report,
          std::to_string(
              report.failedRoomCount) +
              " room(s) are in a failed state");
    }

    if (report.transitioningRoomCount != 0 &&
        report.serverStatus == ServerStatus::Running)
    {
      mark_degraded(
          report,
          std::to_string(
              report.transitioningRoomCount) +
              " room(s) are changing lifecycle state");
    }

    if (report.closedRoomCount != 0 &&
        report.serverStatus == ServerStatus::Running)
    {
      mark_degraded(
          report,
          std::to_string(
              report.closedRoomCount) +
              " closed room(s) remain registered");
    }

    if (options_.maxQueuedCommands != 0 &&
        report.queuedCommandCount >
            options_.maxQueuedCommands)
    {
      mark_degraded(
          report,
          "queued command count exceeds the configured health threshold");
    }

    const std::vector<SessionId> sessionIds =
        manager->session_ids();

    report.sessionCount =
        sessionIds.size();

    for (const auto &sessionId : sessionIds)
    {
      SessionPtr session =
          manager->find_session(sessionId);

      if (!session)
      {
        mark_unhealthy(
            report,
            "session registry changed during health inspection");

        continue;
      }

      switch (session->status())
      {
      case SessionStatus::Connected:
        ++report.connectedSessionCount;
        break;

      case SessionStatus::Detached:
        ++report.detachedSessionCount;
        break;

      case SessionStatus::Closed:
        ++report.closedSessionCount;
        break;
      }
    }

    if (report.closedSessionCount != 0)
    {
      mark_degraded(
          report,
          std::to_string(
              report.closedSessionCount) +
              " closed session(s) remain registered");
    }

    if (options_.degradeOnDetachedSessions &&
        report.detachedSessionCount != 0)
    {
      mark_degraded(
          report,
          std::to_string(
              report.detachedSessionCount) +
              " logical session(s) are detached");
    }

    if (report.presenceStoreAvailable)
    {
      try
      {
        report.presenceCount =
            manager->presence_store()->count();
      }
      catch (const std::exception &error)
      {
        mark_unhealthy(
            report,
            std::string{
                "failed to inspect presence store: "} +
                error.what());
      }
      catch (...)
      {
        mark_unhealthy(
            report,
            "failed to inspect presence store");
      }
    }

    if (metrics_)
    {
      report.metricsAvailable = true;
      report.metrics = metrics_->snapshot();

      if (report.metrics.errors >
          options_.recordedErrorTolerance)
      {
        mark_degraded(
            report,
            "recorded runtime errors exceed the configured tolerance");
      }

      if (report.metrics.protocolErrors >
          options_.protocolErrorTolerance)
      {
        mark_degraded(
            report,
            "recorded protocol errors exceed the configured tolerance");
      }
    }

    return report;
  }

  const ServerPtr &
  HealthMonitor::server() const noexcept
  {
    return server_;
  }

  const MetricsPtr &
  HealthMonitor::metrics() const noexcept
  {
    return metrics_;
  }

  const HealthOptions &
  HealthMonitor::options() const noexcept
  {
    return options_;
  }

  void HealthMonitor::mark_degraded(
      HealthReport &report,
      std::string issue)
  {
    if (report.status == HealthStatus::Healthy)
    {
      report.status =
          HealthStatus::Degraded;
    }

    report.issues.push_back(
        std::move(issue));
  }

  void HealthMonitor::mark_unhealthy(
      HealthReport &report,
      std::string issue)
  {
    report.status =
        HealthStatus::Unhealthy;

    report.issues.push_back(
        std::move(issue));
  }

} // namespace vix::realtime
