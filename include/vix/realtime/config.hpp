/**
 *
 * @file config.hpp
 * @author Gaspard Kirira
 * @brief Runtime configuration for the Vix Realtime module.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_CONFIG_HPP
#define VIX_REALTIME_CONFIG_HPP

#include <chrono>
#include <cstddef>

#include <vix/config/Config.hpp>
#include <vix/realtime/api.hpp>

namespace vix::realtime
{
  /**
   * @brief Runtime limits and lifecycle policies for Realtime rooms.
   *
   * The configuration applies to the generic Realtime runtime and remains
   * independent of application-specific room state and business rules.
   */
  struct VIX_REALTIME_API Config
  {
    /** @brief Maximum number of rooms active in the current process. */
    std::size_t maxActiveRooms{1000};

    /** @brief Maximum number of logical sessions managed by the runtime. */
    std::size_t maxSessions{10000};

    /** @brief Maximum number of sessions allowed in one room. */
    std::size_t maxSessionsPerRoom{256};

    /** @brief Maximum number of rooms one logical session may join. */
    std::size_t maxRoomsPerSession{32};

    /** @brief Maximum number of commands waiting in one room queue. */
    std::size_t maxPendingCommandsPerRoom{1024};

    /** @brief Maximum accepted serialized protocol payload size in bytes. */
    std::size_t maxPayloadSize{64 * 1024};

    /** @brief Maximum number of events returned during one replay. */
    std::size_t maxReplayEvents{1000};

    /** @brief Maximum serialized size of one replay in bytes. */
    std::size_t maxReplayBytes{4 * 1024 * 1024};

    /** @brief Maximum number of rooms restored by one resume request. */
    std::size_t maxResumeRooms{32};

    /** @brief Number of events between automatic snapshots. */
    std::size_t snapshotEveryEvents{100};

    /** @brief Number of recent snapshots retained for each room. */
    std::size_t snapshotsToKeep{3};

    /**
     * @brief Inactivity period before an empty room may be closed.
     *
     * A zero duration disables automatic idle room closure.
     */
    std::chrono::milliseconds roomIdleTimeout{300000};

    /**
     * @brief Maximum duration allowed for one command handler.
     *
     * A zero duration disables handler timeout enforcement.
     */
    std::chrono::milliseconds commandTimeout{5000};

    /**
     * @brief Maximum duration allowed for loading or restoring a room.
     */
    std::chrono::milliseconds roomOpenTimeout{10000};

    /**
     * @brief Lifetime of a temporarily disconnected logical session.
     */
    std::chrono::seconds sessionResumeWindow{120};

    /**
     * @brief Interval expected between presence heartbeats.
     */
    std::chrono::seconds presenceHeartbeatInterval{30};

    /**
     * @brief Duration after which stale presence records are removed.
     */
    std::chrono::seconds presenceTimeout{90};

    /**
     * @brief Maximum duration allowed for replay processing.
     */
    std::chrono::milliseconds replayTimeout{5000};

    /** @brief Create a snapshot when a room closes cleanly. */
    bool snapshotOnRoomClose{true};

    /** @brief Restore room state automatically when a room is opened. */
    bool restoreRoomsOnOpen{true};

    /** @brief Enable session resume and missing-event replay. */
    bool enableSessionResume{true};

    /** @brief Enable local presence tracking. */
    bool enablePresence{true};

    /**
     * @brief Build a Realtime configuration from the Vix core configuration.
     *
     * Recognized values use the `realtime.` prefix.
     *
     * @param core Core Vix configuration source.
     * @return Resolved Realtime configuration.
     */
    [[nodiscard]] static Config from_core(
        const vix::config::Config &core);

    /**
     * @brief Validate all configured limits and durations.
     *
     * Throws `vix::realtime::Error` with
     * `ErrorCode::InvalidConfiguration` when a value is inconsistent.
     */
    void validate() const;
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_CONFIG_HPP
