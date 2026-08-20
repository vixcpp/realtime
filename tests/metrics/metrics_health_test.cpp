/** @file metrics_health_test.cpp */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>

#include <vix/realtime/command_result.hpp>
#include <vix/realtime/health.hpp>
#include <vix/realtime/metrics.hpp>
#include <vix/realtime/server.hpp>

namespace vix::realtime
{
  TEST(MetricsTest, RecordsRuntimeCountersWhenExplicitlyInstrumented)
  {
    Metrics metrics;
    metrics.set_active_rooms(2);
    metrics.set_active_sessions(3);
    metrics.set_attached_connections(2);
    metrics.set_queued_commands(1);
    metrics.record_command_enqueued();
    metrics.record_command_result(CommandStatus::Accepted, std::chrono::microseconds{4});
    metrics.record_events_persisted(2);
    metrics.record_snapshot_created();
    metrics.record_snapshot_restored();
    metrics.record_replay(2, 48);
    metrics.record_resume_attempt(true);
    metrics.record_resume_attempt(false);
    metrics.record_protocol_error();
    metrics.record_error();

    const MetricsSnapshot snapshot = metrics.snapshot();
    EXPECT_EQ(snapshot.activeRooms, 2U);
    EXPECT_EQ(snapshot.activeSessions, 3U);
    EXPECT_EQ(snapshot.attachedConnections, 2U);
    EXPECT_EQ(snapshot.queuedCommands, 1U);
    EXPECT_EQ(snapshot.commandsEnqueued, 1U);
    EXPECT_EQ(snapshot.commandsAccepted, 1U);
    EXPECT_EQ(snapshot.eventsPersisted, 2U);
    EXPECT_EQ(snapshot.snapshotsCreated, 1U);
    EXPECT_EQ(snapshot.snapshotsRestored, 1U);
    EXPECT_EQ(snapshot.replayOperations, 1U);
    EXPECT_EQ(snapshot.replayEventsApplied, 2U);
    EXPECT_EQ(snapshot.resumeAttempts, 2U);
    EXPECT_EQ(snapshot.resumeSucceeded, 1U);
    EXPECT_EQ(snapshot.resumeFailed, 1U);
    EXPECT_EQ(snapshot.protocolErrors, 1U);
    EXPECT_EQ(snapshot.errors, 1U);
  }

  TEST(HealthMonitorTest, ReportsExplicitMetricsAndErrorThresholds)
  {
    auto metrics = std::make_shared<Metrics>();
    metrics->record_error();
    metrics->record_protocol_error();
    auto server = std::make_shared<Server>(NodeId{std::string_view{"metrics-node"}});
    ASSERT_TRUE(server->start());

    HealthOptions options;
    options.recordedErrorTolerance = 0;
    options.protocolErrorTolerance = 0;
    HealthMonitor monitor{server, metrics, options};
    const HealthReport report = monitor.check();

    EXPECT_TRUE(report.metricsAvailable);
    EXPECT_EQ(report.metrics.errors, 1U);
    EXPECT_EQ(report.metrics.protocolErrors, 1U);
    EXPECT_EQ(report.status, HealthStatus::Degraded);
  }
} // namespace vix::realtime
