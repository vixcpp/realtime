/**
 *
 * @file room_version.hpp
 * @author Gaspard Kirira
 * @brief Strong monotonic version type for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_VERSION_HPP
#define VIX_REALTIME_ROOM_VERSION_HPP

#include <compare>
#include <cstdint>
#include <string>

#include <vix/realtime/api.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Strong monotonic version associated with a room state.
   *
   * A newly created room starts at version zero. Each persisted event applied
   * to the room advances the version exactly once.
   */
  class VIX_REALTIME_API RoomVersion
  {
  public:
    /**
     * @brief Underlying integral value type.
     */
    using value_type = VersionValue;

    /**
     * @brief Initial version assigned to a new room.
     */
    static constexpr value_type initial_value = 0;

    /**
     * @brief Construct the initial room version.
     */
    constexpr RoomVersion() noexcept = default;

    /**
     * @brief Construct a room version from an integral value.
     *
     * @param value Non-negative version value.
     *
     * @throws vix::realtime::Error
     *         When the supplied value is negative.
     */
    explicit RoomVersion(value_type value);

    /**
     * @brief Return the underlying version value.
     *
     * @return Current room version.
     */
    [[nodiscard]] constexpr value_type value() const noexcept
    {
      return value_;
    }

    /**
     * @brief Return whether this is the initial room version.
     *
     * @return True when the version equals zero.
     */
    [[nodiscard]] constexpr bool is_initial() const noexcept
    {
      return value_ == initial_value;
    }

    /**
     * @brief Return the next room version.
     *
     * @return Version immediately following the current version.
     *
     * @throws vix::realtime::Error
     *         When the version cannot be incremented.
     */
    [[nodiscard]] RoomVersion next() const;

    /**
     * @brief Advance the room version by one.
     *
     * @return Reference to the updated version.
     *
     * @throws vix::realtime::Error
     *         When the version cannot be incremented.
     */
    RoomVersion &increment();

    /**
     * @brief Compare two room versions.
     */
    auto operator<=>(const RoomVersion &) const noexcept = default;

  private:
    /** @brief Current monotonic room version. */
    value_type value_{initial_value};
  };

  /**
   * @brief Convert a room version to its decimal representation.
   *
   * @param version Room version to convert.
   * @return Decimal version string.
   */
  [[nodiscard]] VIX_REALTIME_API std::string
  to_string(RoomVersion version);

} // namespace vix::realtime

#endif // VIX_REALTIME_ROOM_VERSION_HPP
