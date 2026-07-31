/**
 *
 * @file room_owner.hpp
 * @author Gaspard Kirira
 * @brief Room ownership descriptor for Vix Realtime nodes.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_OWNER_HPP
#define VIX_REALTIME_ROOM_OWNER_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Monotonic generation identifying one room ownership claim.
   *
   * A newer claim must always use a generation greater than the previous
   * ownership generation for the same room.
   */
  using RoomOwnerGeneration = std::uint64_t;

  /**
   * @brief Lifecycle state of one room ownership claim.
   */
  enum class RoomOwnerStatus : std::uint8_t
  {
    /** @brief The node currently owns and may operate the room. */
    Active = 0,

    /** @brief Ownership is being released and accepts no new work. */
    Releasing,

    /** @brief Ownership was permanently released. */
    Released
  };

  /**
   * @brief Return the stable textual representation of an ownership status.
   *
   * @param status Ownership status.
   * @return Stable lowercase identifier.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(RoomOwnerStatus status) noexcept
  {
    switch (status)
    {
    case RoomOwnerStatus::Active:
      return "active";

    case RoomOwnerStatus::Releasing:
      return "releasing";

    case RoomOwnerStatus::Released:
      return "released";
    }

    return "released";
  }

  /**
   * @brief Describes which runtime node owns one logical room.
   *
   * A room owner contains:
   *
   * - the logical room identifier;
   * - the owning runtime node;
   * - a monotonic ownership generation;
   * - optional lease expiration;
   * - release lifecycle information.
   *
   * The object is intentionally copyable so a `RoomDirectory` may safely
   * return ownership snapshots without exposing its internal synchronization.
   */
  class VIX_REALTIME_API RoomOwner
  {
  public:
    /**
     * @brief Construct an empty ownership descriptor.
     *
     * The resulting descriptor is invalid until replaced by a complete
     * `RoomOwner` value.
     */
    RoomOwner() = default;

    /**
     * @brief Construct an active room ownership claim.
     *
     * @param roomId Owned logical room.
     * @param nodeId Node owning the room.
     * @param generation Monotonic non-zero ownership generation.
     * @param acquiredAt Time at which ownership was acquired.
     * @param expiresAt Optional lease expiration timestamp.
     * @param metadata Non-authoritative ownership metadata.
     *
     * @throws vix::realtime::Error
     *         When identifiers, generation, or timestamps are invalid.
     */
    RoomOwner(
        RoomId roomId,
        NodeId nodeId,
        RoomOwnerGeneration generation = 1,
        Timestamp acquiredAt = SystemClock::now(),
        std::optional<Timestamp> expiresAt = std::nullopt,
        JsonObject metadata = {});

    explicit RoomOwner(NodeId nodeId);

    void set_node_id(NodeId nodeId);

    [[nodiscard]] bool acquire(const RoomId &roomId);

    [[nodiscard]] bool claim(const RoomId &roomId);

    [[nodiscard]] bool release(const RoomId &roomId);

    [[nodiscard]] bool owns(const RoomId &roomId) const;

    [[nodiscard]] bool owns_room(const RoomId &roomId) const;

    [[nodiscard]] bool contains(const RoomId &roomId) const;

    [[nodiscard]] bool has_room(const RoomId &roomId) const;

    [[nodiscard]] std::size_t room_count() const;

    [[nodiscard]] bool empty() const;

    [[nodiscard]] std::vector<RoomId> rooms() const;

    void clear();

    /**
     * @brief Create a leased ownership claim.
     *
     * @param roomId Owned logical room.
     * @param nodeId Node owning the room.
     * @param generation Monotonic non-zero ownership generation.
     * @param acquiredAt Time at which ownership is acquired.
     * @param leaseDuration Positive ownership lease duration.
     * @param metadata Non-authoritative ownership metadata.
     * @return Active leased room ownership descriptor.
     *
     * @throws vix::realtime::Error
     *         When the lease duration is not positive.
     */
    [[nodiscard]] static RoomOwner leased(
        RoomId roomId,
        NodeId nodeId,
        RoomOwnerGeneration generation,
        Timestamp acquiredAt,
        std::chrono::milliseconds leaseDuration,
        JsonObject metadata = {});

    /**
     * @brief Return the owned room identifier.
     *
     * @return Room identifier.
     */
    [[nodiscard]] const RoomId &room_id() const noexcept;

    /**
     * @brief Return the owning runtime node.
     *
     * @return Node identifier.
     */
    [[nodiscard]] const NodeId &node_id() const noexcept;

    /**
     * @brief Return the ownership generation.
     *
     * @return Monotonic ownership generation.
     */
    [[nodiscard]] RoomOwnerGeneration generation() const noexcept;

    /**
     * @brief Return the next ownership generation.
     *
     * @return Current generation incremented by one.
     *
     * @throws vix::realtime::Error
     *         When the generation reached its maximum value.
     */
    [[nodiscard]] RoomOwnerGeneration next_generation() const;

    /**
     * @brief Return the ownership lifecycle status.
     *
     * @return Ownership status.
     */
    [[nodiscard]] RoomOwnerStatus status() const noexcept;

    /**
     * @brief Return the ownership acquisition timestamp.
     *
     * @return Acquisition timestamp.
     */
    [[nodiscard]] Timestamp acquired_at() const noexcept;

    /**
     * @brief Return the latest lease renewal timestamp.
     *
     * The value initially equals `acquired_at()`.
     *
     * @return Latest renewal timestamp.
     */
    [[nodiscard]] Timestamp renewed_at() const noexcept;

    /**
     * @brief Return the optional lease expiration timestamp.
     *
     * An absent timestamp represents ownership without automatic expiration.
     *
     * @return Lease expiration timestamp, when present.
     */
    [[nodiscard]] const std::optional<Timestamp> &
    expires_at() const noexcept;

    /**
     * @brief Return the optional release timestamp.
     *
     * @return Release timestamp when ownership is released.
     */
    [[nodiscard]] const std::optional<Timestamp> &
    released_at() const noexcept;

    /**
     * @brief Return whether this ownership uses an expiring lease.
     *
     * @return True when an expiration timestamp is present.
     */
    [[nodiscard]] bool has_lease() const noexcept;

    /**
     * @brief Return whether the lease has expired.
     *
     * Released ownership is also considered expired.
     *
     * @param now Current timestamp.
     * @return True when the claim may no longer operate the room.
     */
    [[nodiscard]] bool expired(
        Timestamp now = SystemClock::now()) const noexcept;

    /**
     * @brief Return whether the ownership claim is currently active.
     *
     * @param now Current timestamp.
     * @return True when active, unreleased, and not expired.
     */
    [[nodiscard]] bool active(
        Timestamp now = SystemClock::now()) const noexcept;

    /**
     * @brief Return whether a node currently owns the room.
     *
     * @param nodeId Candidate owner node.
     * @param now Current timestamp.
     * @return True when the node matches an active ownership claim.
     */
    [[nodiscard]] bool owned_by(
        const NodeId &nodeId,
        Timestamp now = SystemClock::now()) const noexcept;

    /**
     * @brief Return whether a node and generation identify this claim.
     *
     * This comparison does not evaluate expiration.
     *
     * @param nodeId Candidate owner node.
     * @param generation Candidate ownership generation.
     * @return True when both values match.
     */
    [[nodiscard]] bool matches(
        const NodeId &nodeId,
        RoomOwnerGeneration generation) const noexcept;

    /**
     * @brief Renew the active ownership lease.
     *
     * The new expiration is calculated from `now`.
     *
     * @param now Renewal timestamp.
     * @param leaseDuration Positive lease duration.
     * @return Current ownership descriptor.
     *
     * @throws vix::realtime::Error
     *         When ownership is not active, already expired, or the duration
     *         is not positive.
     */
    RoomOwner &renew(
        Timestamp now,
        std::chrono::milliseconds leaseDuration);

    /**
     * @brief Convert active ownership to a non-expiring claim.
     *
     * @param now Timestamp assigned to the ownership update.
     * @return Current ownership descriptor.
     *
     * @throws vix::realtime::Error
     *         When ownership is not active or already expired.
     */
    RoomOwner &clear_expiration(
        Timestamp now = SystemClock::now());

    /**
     * @brief Mark ownership as being released.
     *
     * Releasing ownership is no longer considered active.
     *
     * @param now Release transition timestamp.
     * @return Current ownership descriptor.
     *
     * @throws vix::realtime::Error
     *         When the timestamp precedes acquisition or renewal.
     */
    RoomOwner &begin_release(
        Timestamp now = SystemClock::now());

    /**
     * @brief Permanently release ownership.
     *
     * Calling this method on an already released descriptor is harmless.
     *
     * @param now Final release timestamp.
     * @return Current ownership descriptor.
     *
     * @throws vix::realtime::Error
     *         When the timestamp precedes acquisition or renewal.
     */
    RoomOwner &release(
        Timestamp now = SystemClock::now());

    /**
     * @brief Return non-authoritative ownership metadata.
     *
     * @return Constant reference to ownership metadata.
     */
    [[nodiscard]] const JsonObject &
    metadata() const noexcept;

    /**
     * @brief Replace non-authoritative ownership metadata.
     *
     * @param value New ownership metadata.
     * @return Current ownership descriptor.
     */
    RoomOwner &set_metadata(JsonObject value);

    /**
     * @brief Return whether the ownership descriptor is consistent.
     *
     * @return True when all identifiers, timestamps, and lifecycle fields are
     *         valid.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /**
     * @brief Validate the ownership descriptor.
     *
     * @throws vix::realtime::Error
     *         When the descriptor contains inconsistent state.
     */
    void validate() const;

  private:
    /** @brief Owned logical room. */
    RoomId roomId_{};

    /** @brief Runtime node owning the room. */
    NodeId nodeId_{};

    /** @brief Monotonic ownership claim generation. */
    RoomOwnerGeneration generation_{0};

    /** @brief Ownership lifecycle status. */
    RoomOwnerStatus status_{RoomOwnerStatus::Released};

    /** @brief Time at which ownership was acquired. */
    Timestamp acquiredAt_{};

    /** @brief Time of the latest ownership renewal. */
    Timestamp renewedAt_{};

    /** @brief Optional ownership lease expiration. */
    std::optional<Timestamp> expiresAt_{};

    /** @brief Optional permanent release timestamp. */
    std::optional<Timestamp> releasedAt_{};

    /** @brief Non-authoritative ownership metadata. */
    JsonObject metadata_{};

    /** @brief Compatibility multi-room ownership set. */
    std::set<RoomId> ownedRooms_{};
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_ROOM_OWNER_HPP
