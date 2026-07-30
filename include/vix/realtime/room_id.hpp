/**
 *
 * @file room_id.hpp
 * @author Gaspard Kirira
 * @brief Strong identifier type for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_ID_HPP
#define VIX_REALTIME_ROOM_ID_HPP

#include <compare>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include <vix/realtime/api.hpp>

namespace vix::realtime
{
  /**
   * @brief Strong and validated identifier for a Realtime room.
   *
   * A room identifier is an application-defined stable key such as:
   *
   * @code
   * house:abc123
   * city:global
   * game:lobby-42
   * @endcode
   *
   * Valid identifiers:
   *
   * - are not empty;
   * - contain at most `max_size` characters;
   * - contain only ASCII letters, digits, `_`, `-`, `.`, `/`, or `:`;
   * - do not begin or end with a separator;
   * - do not contain consecutive separators.
   */
  class VIX_REALTIME_API RoomId
  {
  public:
    /**
     * @brief Maximum number of characters allowed in a room identifier.
     */
    static constexpr std::size_t max_size = 128;

    /**
     * @brief Construct an empty room identifier.
     *
     * The empty value represents an uninitialized identifier and is not valid
     * for room creation, lookup, persistence, or protocol messages.
     */
    RoomId() = default;

    /**
     * @brief Construct and validate a room identifier.
     *
     * The supplied value is copied into the identifier.
     *
     * @param value Room identifier value.
     *
     * @throws vix::realtime::Error
     *         When the supplied identifier is invalid.
     */
    explicit RoomId(std::string_view value);

    /**
     * @brief Return the stored identifier.
     *
     * @return Constant reference to the room identifier string.
     */
    [[nodiscard]] const std::string &value() const noexcept;

    /**
     * @brief Return a non-owning view of the stored identifier.
     *
     * @return String view of the room identifier.
     */
    [[nodiscard]] std::string_view view() const noexcept;

    /**
     * @brief Return whether the identifier contains no value.
     *
     * @return True when the identifier is empty.
     */
    [[nodiscard]] bool empty() const noexcept;

    /**
     * @brief Return the number of characters in the identifier.
     *
     * @return Identifier size.
     */
    [[nodiscard]] std::size_t size() const noexcept;

    /**
     * @brief Return whether a string is a valid room identifier.
     *
     * This function performs validation without throwing.
     *
     * @param value Candidate identifier.
     * @return True when the candidate is valid.
     */
    [[nodiscard]] static bool is_valid(
        std::string_view value) noexcept;

    /**
     * @brief Validate a room identifier.
     *
     * @param value Candidate identifier.
     *
     * @throws vix::realtime::Error
     *         When the candidate is invalid.
     */
    static void validate(
        std::string_view value);

    /**
     * @brief Compare two room identifiers.
     */
    auto operator<=>(const RoomId &) const noexcept = default;

  private:
    /** @brief Validated room identifier value. */
    std::string value_{};
  };

  /**
   * @brief Return the textual value of a room identifier.
   *
   * @param roomId Room identifier.
   * @return String view referencing the identifier value.
   */
  [[nodiscard]] VIX_REALTIME_API std::string_view
  to_string(const RoomId &roomId) noexcept;

} // namespace vix::realtime

namespace std
{
  /**
   * @brief Hash specialization for `vix::realtime::RoomId`.
   */
  template <>
  struct hash<vix::realtime::RoomId>
  {
    /**
     * @brief Compute the hash of a room identifier.
     *
     * @param roomId Room identifier.
     * @return Hash value.
     */
    [[nodiscard]] std::size_t operator()(
        const vix::realtime::RoomId &roomId) const noexcept
    {
      return std::hash<std::string_view>{}(
          roomId.view());
    }
  };

} // namespace std

#endif // VIX_REALTIME_ROOM_ID_HPP
