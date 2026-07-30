/**
 *
 * @file distributed_presence.hpp
 * @author Gaspard Kirira
 * @brief Distributed presence contract for multi-node Vix Realtime runtimes.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_DISTRIBUTED_PRESENCE_HPP
#define VIX_REALTIME_DISTRIBUTED_PRESENCE_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/presence_store.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Operational state of a distributed presence backend.
   */
  enum class DistributedPresenceStatus : std::uint8_t
  {
    /** @brief The backend is reachable and operating normally. */
    Healthy = 0,

    /** @brief The backend remains usable but has partial failures. */
    Degraded,

    /** @brief The backend is currently unavailable. */
    Unavailable
  };

  /**
   * @brief Return the stable textual representation of a backend status.
   *
   * @param status Distributed presence status.
   * @return Stable lowercase identifier.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(
      DistributedPresenceStatus status) noexcept
  {
    switch (status)
    {
    case DistributedPresenceStatus::Healthy:
      return "healthy";

    case DistributedPresenceStatus::Degraded:
      return "degraded";

    case DistributedPresenceStatus::Unavailable:
      return "unavailable";
    }

    return "unavailable";
  }

  /**
   * @brief Presence information associated with one runtime node.
   */
  struct DistributedPresenceNode
  {
    /** @brief Runtime node identifier. */
    NodeId nodeId{};

    /** @brief Most recent heartbeat received from the node. */
    Timestamp lastSeen{SystemClock::now()};

    /** @brief Whether this entry represents the local node. */
    bool local{false};

    /** @brief Non-authoritative node metadata. */
    JsonObject metadata{};

    /**
     * @brief Return whether the node heartbeat has expired.
     *
     * @param now Current timestamp.
     * @param timeout Maximum accepted heartbeat age.
     * @return True when the node should be considered stale.
     */
    [[nodiscard]] bool stale(
        Timestamp now,
        std::chrono::milliseconds timeout) const noexcept
    {
      if (timeout.count() <= 0)
      {
        return true;
      }

      if (now < lastSeen)
      {
        return false;
      }

      return now - lastSeen >= timeout;
    }
  };

  /**
   * @brief Presence store synchronized across multiple runtime nodes.
   *
   * A distributed presence backend implements the complete `PresenceStore`
   * contract while additionally tracking node heartbeats and backend health.
   *
   * Implementations may use PostgreSQL, Redis, a message broker, or another
   * shared coordination system. Presence remains ephemeral and must not be
   * treated as authoritative application state.
   */
  class VIX_REALTIME_API DistributedPresence
      : public PresenceStore
  {
  public:
    /**
     * @brief Destroy the distributed presence backend.
     */
    ~DistributedPresence() override = default;

    /**
     * @brief Return the local runtime node identifier.
     *
     * @return Local node identifier.
     */
    [[nodiscard]] virtual const NodeId &
    local_node_id() const noexcept = 0;

    /**
     * @brief Publish or refresh the local node heartbeat.
     *
     * @param now Heartbeat timestamp.
     * @param metadata Non-authoritative local node metadata.
     *
     * @throws vix::realtime::Error
     *         When the heartbeat cannot be persisted.
     */
    virtual void heartbeat(
        Timestamp now = SystemClock::now(),
        JsonObject metadata = {}) = 0;

    /**
     * @brief Return one known runtime node.
     *
     * @param nodeId Node identifier.
     * @return Node presence, or no value when unknown.
     */
    [[nodiscard]] virtual std::optional<DistributedPresenceNode>
    find_node(
        const NodeId &nodeId) const = 0;

    /**
     * @brief Return every known runtime node.
     *
     * Results should be ordered deterministically by node identifier.
     *
     * @return Known node presence records.
     */
    [[nodiscard]] virtual std::vector<DistributedPresenceNode>
    nodes() const = 0;

    /**
     * @brief Return nodes whose heartbeat remains active.
     *
     * @param now Current timestamp.
     * @param timeout Maximum accepted heartbeat age.
     * @return Active node presence records.
     */
    [[nodiscard]] virtual std::vector<DistributedPresenceNode>
    active_nodes(
        Timestamp now,
        std::chrono::milliseconds timeout) const = 0;

    /**
     * @brief Return whether one runtime node is active.
     *
     * @param nodeId Node identifier.
     * @param now Current timestamp.
     * @param timeout Maximum accepted heartbeat age.
     * @return True when the node exists and its heartbeat is recent.
     */
    [[nodiscard]] virtual bool node_active(
        const NodeId &nodeId,
        Timestamp now,
        std::chrono::milliseconds timeout) const = 0;

    /**
     * @brief Remove stale runtime node heartbeat records.
     *
     * Presence records owned by removed nodes may also be removed depending on
     * backend policy.
     *
     * @param now Current timestamp.
     * @param timeout Maximum accepted heartbeat age.
     * @return Number of removed node records.
     */
    virtual std::size_t prune_stale_nodes(
        Timestamp now,
        std::chrono::milliseconds timeout) = 0;

    /**
     * @brief Remove all presence owned by one runtime node.
     *
     * @param nodeId Runtime node identifier.
     * @return Number of removed presence records.
     */
    virtual std::size_t clear_node(
        const NodeId &nodeId) = 0;

    /**
     * @brief Return the current backend operational status.
     *
     * @return Distributed presence backend status.
     */
    [[nodiscard]] virtual DistributedPresenceStatus
    distributed_status() const noexcept = 0;

    /**
     * @brief Test whether the shared presence backend is reachable.
     *
     * @return True when the backend responds successfully.
     */
    [[nodiscard]] virtual bool ping() const noexcept = 0;
  };

  /**
   * @brief Shared ownership pointer for distributed presence.
   */
  using DistributedPresencePtr =
      std::shared_ptr<DistributedPresence>;

} // namespace vix::realtime

#endif // VIX_REALTIME_DISTRIBUTED_PRESENCE_HPP
