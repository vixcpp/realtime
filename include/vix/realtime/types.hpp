/**
 *
 * @file types.hpp
 * @author Gaspard Kirira
 * @brief Common types used by the Vix Realtime module.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_TYPES_HPP
#define VIX_REALTIME_TYPES_HPP

#include <chrono>
#include <cstdint>
#include <string>

#include <vix/json/Simple.hpp>

namespace vix::realtime
{
  /**
   * @brief JSON object used for commands, events, state, and metadata.
   */
  using JsonObject = vix::json::kvs;

  /**
   * @brief Signed integral type used for monotonic room versions.
   */
  using VersionValue = std::int64_t;

  /**
   * @brief Signed integral type used for ordered event identifiers.
   */
  using EventIdValue = std::int64_t;

  /**
   * @brief Wall clock used for persistent timestamps.
   */
  using SystemClock = std::chrono::system_clock;

  /**
   * @brief Monotonic clock used for runtime timeouts and activity tracking.
   */
  using SteadyClock = std::chrono::steady_clock;

  /**
   * @brief Persistent timestamp attached to commands, events, and snapshots.
   */
  using Timestamp = SystemClock::time_point;

  /**
   * @brief Monotonic timestamp used for local lifecycle management.
   */
  using SteadyTimestamp = SteadyClock::time_point;

  /**
   * @brief Generic application identity attached to a logical session.
   *
   * The Realtime module does not interpret this value. Applications may use
   * user identifiers, anonymous identities, service accounts, or another
   * stable identity representation.
   */
  using Identity = std::string;

  /**
   * @brief Identifier of a physical transport connection.
   */
  using ConnectionId = std::string;

  /**
   * @brief Identifier used to correlate a command with resulting events.
   */
  using CorrelationId = std::string;

  /**
   * @brief Identifier supplied by a client for command idempotency.
   */
  using RequestId = std::string;

  /**
   * @brief Opaque token used to restore a logical session.
   */
  using ResumeToken = std::string;

  /**
   * @brief Schema version used by events and snapshots.
   */
  using SchemaVersion = std::uint32_t;

} // namespace vix::realtime

#endif // VIX_REALTIME_TYPES_HPP
