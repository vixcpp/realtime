/**
 *
 * @file room_directory.hpp
 * @author Gaspard Kirira
 * @brief Thread-safe room ownership directory for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_DIRECTORY_HPP
#define VIX_REALTIME_ROOM_DIRECTORY_HPP

#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_owner.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Thread-safe directory mapping logical rooms to owner nodes.
   *
   * The directory stores at most one current ownership claim per room. Every
   * new claim receives a monotonic generation greater than all previous claims
   * observed for that room.
   *
   * Expired and released claims are not returned by `resolve()`. Generation
   * history is preserved after a claim is removed so a later owner cannot
   * accidentally reuse an older generation.
   *
   * This implementation is process-local. A future distributed directory may
   * implement the same ownership semantics over an external coordination
   * system.
   */
  class VIX_REALTIME_API RoomDirectory
  {
  public:
    /**
     * @brief Construct an empty room ownership directory.
     */
    RoomDirectory() = default;

    /**
     * @brief Destroy the room ownership directory.
     */
    ~RoomDirectory() = default;

    RoomDirectory(const RoomDirectory &) = delete;
    RoomDirectory &operator=(const RoomDirectory &) = delete;
    RoomDirectory(RoomDirectory &&) = delete;
    RoomDirectory &operator=(RoomDirectory &&) = delete;

    /**
     * @brief Acquire ownership of a room.
     *
     * The directory assigns the next monotonic generation for the room.
     * Acquisition fails while another unexpired claim exists, including a
     * claim currently being released.
     *
     * An absent lease duration creates non-expiring ownership.
     *
     * @param roomId Room to acquire.
     * @param nodeId Node acquiring the room.
     * @param leaseDuration Optional positive ownership lease duration.
     * @param now Acquisition timestamp.
     * @param metadata Non-authoritative ownership metadata.
     * @return Newly created ownership claim.
     *
     * @throws vix::realtime::Error
     *         When identifiers or lease duration are invalid, or another owner
     *         still holds the room.
     */
    [[nodiscard]] RoomOwner acquire(
        RoomId roomId,
        NodeId nodeId,
        std::optional<std::chrono::milliseconds> leaseDuration = std::nullopt,
        Timestamp now = SystemClock::now(),
        JsonObject metadata = {});

    /**
     * @brief Register an externally created ownership claim.
     *
     * This operation supports future distributed coordination adapters. The
     * incoming claim must be active and use a generation newer than every
     * generation already observed for the room.
     *
     * @param owner Ownership claim to register.
     * @param now Current timestamp used to reject expired claims.
     * @return Registered ownership claim.
     *
     * @throws vix::realtime::Error
     *         When the claim is invalid, inactive, expired, or stale.
     */
    [[nodiscard]] RoomOwner register_owner(
        RoomOwner owner,
        Timestamp now = SystemClock::now());

    /**
     * @brief Renew an active leased ownership claim.
     *
     * The room, owner node, and generation must match the current directory
     * entry.
     *
     * @param roomId Owned room.
     * @param nodeId Current owner node.
     * @param generation Current ownership generation.
     * @param leaseDuration Positive lease duration beginning at `now`.
     * @param now Renewal timestamp.
     * @return Renewed ownership claim.
     *
     * @throws vix::realtime::Error
     *         When the claim does not exist, does not match, or cannot be
     *         renewed.
     */
    [[nodiscard]] RoomOwner renew(
        const RoomId &roomId,
        const NodeId &nodeId,
        RoomOwnerGeneration generation,
        std::chrono::milliseconds leaseDuration,
        Timestamp now = SystemClock::now());

    /**
     * @brief Convert an active claim to non-expiring ownership.
     *
     * @param roomId Owned room.
     * @param nodeId Current owner node.
     * @param generation Current ownership generation.
     * @param now Ownership update timestamp.
     * @return Updated ownership claim.
     *
     * @throws vix::realtime::Error
     *         When the current claim does not exist or does not match.
     */
    [[nodiscard]] RoomOwner make_permanent(
        const RoomId &roomId,
        const NodeId &nodeId,
        RoomOwnerGeneration generation,
        Timestamp now = SystemClock::now());

    /**
     * @brief Transfer active room ownership to another node.
     *
     * Transfer atomically replaces the current owner with a new claim using
     * the next monotonic generation.
     *
     * An absent lease duration creates non-expiring ownership for the new
     * owner.
     *
     * @param roomId Room being transferred.
     * @param currentNodeId Current owner node.
     * @param currentGeneration Current ownership generation.
     * @param nextNodeId Node receiving ownership.
     * @param leaseDuration Optional positive lease duration.
     * @param now Transfer timestamp.
     * @param metadata Metadata assigned to the new ownership claim.
     * @return New ownership claim.
     *
     * @throws vix::realtime::Error
     *         When the current claim is missing, stale, expired, or mismatched.
     */
    [[nodiscard]] RoomOwner transfer(
        const RoomId &roomId,
        const NodeId &currentNodeId,
        RoomOwnerGeneration currentGeneration,
        NodeId nextNodeId,
        std::optional<std::chrono::milliseconds> leaseDuration = std::nullopt,
        Timestamp now = SystemClock::now(),
        JsonObject metadata = {});

    /**
     * @brief Mark a room ownership claim as releasing.
     *
     * A releasing claim is no longer returned by `resolve()`, but it continues
     * blocking new acquisition until `release()` is called or its lease
     * expires.
     *
     * @param roomId Owned room.
     * @param nodeId Current owner node.
     * @param generation Current ownership generation.
     * @param now Release transition timestamp.
     * @return Updated ownership claim.
     *
     * @throws vix::realtime::Error
     *         When the current claim does not exist or does not match.
     */
    [[nodiscard]] RoomOwner begin_release(
        const RoomId &roomId,
        const NodeId &nodeId,
        RoomOwnerGeneration generation,
        Timestamp now = SystemClock::now());

    /**
     * @brief Permanently release and remove an ownership claim.
     *
     * Generation history is preserved after removal.
     *
     * @param roomId Owned room.
     * @param nodeId Current owner node.
     * @param generation Current ownership generation.
     * @param now Final release timestamp.
     * @return Released ownership descriptor.
     *
     * @throws vix::realtime::Error
     *         When the current claim does not exist, is expired, or does not
     *         match.
     */
    [[nodiscard]] RoomOwner release(
        const RoomId &roomId,
        const NodeId &nodeId,
        RoomOwnerGeneration generation,
        Timestamp now = SystemClock::now());

    /**
     * @brief Resolve the active owner of a room.
     *
     * Releasing, released, and expired claims are not returned.
     *
     * @param roomId Room to resolve.
     * @param now Current timestamp.
     * @return Active ownership claim, or no value.
     */
    [[nodiscard]] std::optional<RoomOwner> resolve(
        const RoomId &roomId,
        Timestamp now = SystemClock::now()) const;

    /**
     * @brief Inspect the currently stored ownership descriptor.
     *
     * Unlike `resolve()`, this method may return an expired or releasing claim.
     *
     * @param roomId Room to inspect.
     * @return Stored ownership claim, or no value.
     */
    [[nodiscard]] std::optional<RoomOwner> inspect(
        const RoomId &roomId) const;

    /**
     * @brief Return whether a node actively owns a room.
     *
     * @param roomId Room to inspect.
     * @param nodeId Candidate owner node.
     * @param now Current timestamp.
     * @return True when the active claim belongs to the node.
     */
    [[nodiscard]] bool owns(
        const RoomId &roomId,
        const NodeId &nodeId,
        Timestamp now = SystemClock::now()) const;

    /**
     * @brief Return whether a node and generation match the active claim.
     *
     * @param roomId Room to inspect.
     * @param nodeId Candidate owner node.
     * @param generation Candidate ownership generation.
     * @param now Current timestamp.
     * @return True when the complete active ownership identity matches.
     */
    [[nodiscard]] bool matches(
        const RoomId &roomId,
        const NodeId &nodeId,
        RoomOwnerGeneration generation,
        Timestamp now = SystemClock::now()) const;

    /**
     * @brief Return every active ownership claim.
     *
     * Results are sorted by room identifier.
     *
     * @param now Current timestamp.
     * @return Active ownership claims.
     */
    [[nodiscard]] std::vector<RoomOwner> active_owners(
        Timestamp now = SystemClock::now()) const;

    /**
     * @brief Return active rooms owned by one node.
     *
     * Results are sorted by room identifier.
     *
     * @param nodeId Owner node to inspect.
     * @param now Current timestamp.
     * @return Active ownership claims belonging to the node.
     *
     * @throws vix::realtime::Error
     *         When the node identifier is empty.
     */
    [[nodiscard]] std::vector<RoomOwner> owned_by(
        const NodeId &nodeId,
        Timestamp now = SystemClock::now()) const;

    /**
     * @brief Return the latest generation observed for a room.
     *
     * The generation remains available after the current claim is removed.
     *
     * @param roomId Room to inspect.
     * @return Latest observed generation, or zero when none was observed.
     */
    [[nodiscard]] RoomOwnerGeneration latest_generation(
        const RoomId &roomId) const;

    /**
     * @brief Return the number of currently stored ownership claims.
     *
     * This count includes active, releasing, and expired claims that have not
     * yet been pruned.
     *
     * @return Stored ownership count.
     */
    [[nodiscard]] std::size_t size() const;

    /**
     * @brief Return the number of active ownership claims.
     *
     * @param now Current timestamp.
     * @return Active ownership count.
     */
    [[nodiscard]] std::size_t active_count(
        Timestamp now = SystemClock::now()) const;

    /**
     * @brief Remove expired ownership claims.
     *
     * Generation history remains preserved for every removed room.
     *
     * @param now Current timestamp.
     * @return Number of removed claims.
     */
    std::size_t prune_expired(
        Timestamp now = SystemClock::now());

    /**
     * @brief Remove the current ownership claim for a room.
     *
     * Generation history remains preserved.
     *
     * @param roomId Room claim to remove.
     * @return True when a claim was removed.
     */
    bool clear_room(const RoomId &roomId);

    /**
     * @brief Remove every claim and all generation history.
     */
    void clear();

  private:
    /**
     * @brief Validate an optional ownership lease duration.
     */
    static void validate_lease(
        const std::optional<std::chrono::milliseconds> &leaseDuration);

    /**
     * @brief Return the next room generation while the directory is locked.
     */
    [[nodiscard]] RoomOwnerGeneration
    next_generation_locked(const RoomId &roomId);

    /**
     * @brief Find and validate the current ownership claim.
     */
    [[nodiscard]] RoomOwner &
    require_owner_locked(
        const RoomId &roomId,
        const NodeId &nodeId,
        RoomOwnerGeneration generation,
        Timestamp now,
        bool requireActive);

    /**
     * @brief Create an ownership claim from optional lease information.
     */
    [[nodiscard]] static RoomOwner create_owner(
        RoomId roomId,
        NodeId nodeId,
        RoomOwnerGeneration generation,
        const std::optional<std::chrono::milliseconds> &leaseDuration,
        Timestamp now,
        JsonObject metadata);

    /** @brief Protects ownership and generation maps. */
    mutable std::mutex mutex_{};

    /** @brief Current ownership claims indexed by room. */
    std::unordered_map<RoomId, RoomOwner> owners_{};

    /** @brief Latest observed generation for every known room. */
    std::unordered_map<RoomId, RoomOwnerGeneration> generations_{};
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_ROOM_DIRECTORY_HPP
