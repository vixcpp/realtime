/**
 *
 * @file event_id.hpp
 * @author Gaspard Kirira
 * @brief Strong ordered identifier type for Vix Realtime events.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_EVENT_ID_HPP
#define VIX_REALTIME_EVENT_ID_HPP

#include <compare>
#include <cstdint>
#include <string>

#include <vix/realtime/api.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Strong ordered identifier for an event inside a room stream.
   *
   * Event identifiers are monotonic within one room. The zero value represents
   * the absence of a persisted event and may be used as an initial replay
   * cursor.
   */
  class VIX_REALTIME_API EventId
  {
  public:
    /**
     * @brief Underlying integral value type.
     */
    using value_type = EventIdValue;

    /**
     * @brief Value representing the absence of a persisted event.
     */
    static constexpr value_type none_value = 0;

    /**
     * @brief Construct an empty event identifier.
     */
    constexpr EventId() noexcept = default;

    /**
     * @brief Construct an event identifier from an integral value.
     *
     * @param value Non-negative event identifier.
     *
     * @throws vix::realtime::Error
     *         When the supplied value is negative.
     */
    explicit EventId(value_type value);

    /**
     * @brief Return the underlying event identifier.
     *
     * @return Current identifier value.
     */
    [[nodiscard]] constexpr value_type value() const noexcept
    {
      return value_;
    }

    /**
     * @brief Return whether this identifier represents no event.
     *
     * @return True when the identifier equals zero.
     */
    [[nodiscard]] constexpr bool empty() const noexcept
    {
      return value_ == none_value;
    }

    /**
     * @brief Return the next ordered event identifier.
     *
     * @return Identifier immediately following the current one.
     *
     * @throws vix::realtime::Error
     *         When the identifier cannot be incremented.
     */
    [[nodiscard]] EventId next() const;

    /**
     * @brief Advance the event identifier by one.
     *
     * @return Reference to the updated identifier.
     *
     * @throws vix::realtime::Error
     *         When the identifier cannot be incremented.
     */
    EventId &increment();

    /**
     * @brief Compare two event identifiers.
     */
    auto operator<=>(const EventId &) const noexcept = default;

  private:
    /** @brief Ordered identifier value. */
    value_type value_{none_value};
  };

  /**
   * @brief Convert an event identifier to its decimal representation.
   *
   * @param eventId Event identifier to convert.
   * @return Decimal identifier string.
   */
  [[nodiscard]] VIX_REALTIME_API std::string
  to_string(EventId eventId);

} // namespace vix::realtime

#endif // VIX_REALTIME_EVENT_ID_HPP
