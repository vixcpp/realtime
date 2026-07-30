/**
 *
 * @file session_id.hpp
 * @author Gaspard Kirira
 * @brief Strong identifier type for Vix Realtime logical sessions.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_SESSION_ID_HPP
#define VIX_REALTIME_SESSION_ID_HPP

#include <compare>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include <vix/realtime/api.hpp>

namespace vix::realtime
{
  /**
   * @brief Strong identifier for a logical Realtime session.
   *
   * A logical session may survive the loss or replacement of its current
   * transport connection. It is therefore distinct from a connection ID.
   *
   * Valid session identifiers:
   *
   * - are not empty;
   * - contain at most `max_size` characters;
   * - contain only ASCII letters, digits, `_`, `-`, or `.`;
   * - do not begin or end with a separator;
   * - do not contain consecutive separators.
   */
  class VIX_REALTIME_API SessionId
  {
  public:
    /**
     * @brief Maximum number of characters allowed in a session identifier.
     */
    static constexpr std::size_t max_size = 128;

    /**
     * @brief Construct an empty session identifier.
     *
     * The empty value represents an uninitialized logical session.
     */
    SessionId() = default;

    /**
     * @brief Construct and validate a session identifier.
     *
     * @param value Session identifier value.
     *
     * @throws vix::realtime::Error
     *         When the supplied identifier is invalid.
     */
    explicit SessionId(std::string value);

    /**
     * @brief Construct and validate a session identifier from a string view.
     *
     * @param value Session identifier value.
     *
     * @throws vix::realtime::Error
     *         When the supplied identifier is invalid.
     */
    explicit SessionId(std::string_view value);

    /**
     * @brief Return the stored identifier.
     *
     * @return Constant reference to the identifier string.
     */
    [[nodiscard]] const std::string &value() const noexcept;

    /**
     * @brief Return a non-owning view of the identifier.
     *
     * @return String view of the identifier.
     */
    [[nodiscard]] std::string_view view() const noexcept;

    /**
     * @brief Return whether the identifier is empty.
     *
     * @return True when no identifier is stored.
     */
    [[nodiscard]] bool empty() const noexcept;

    /**
     * @brief Return the number of characters in the identifier.
     *
     * @return Identifier size.
     */
    [[nodiscard]] std::size_t size() const noexcept;

    /**
     * @brief Return whether a string is a valid session identifier.
     *
     * @param value Candidate identifier.
     * @return True when the candidate is valid.
     */
    [[nodiscard]] static bool is_valid(std::string_view value) noexcept;

    /**
     * @brief Validate a session identifier.
     *
     * @param value Candidate identifier.
     *
     * @throws vix::realtime::Error
     *         When the candidate is invalid.
     */
    static void validate(std::string_view value);

    /**
     * @brief Compare two session identifiers.
     */
    auto operator<=>(const SessionId &) const noexcept = default;

  private:
    /** @brief Validated logical session identifier. */
    std::string value_{};
  };

  /**
   * @brief Return the textual value of a session identifier.
   *
   * @param sessionId Session identifier.
   * @return String view referencing the identifier value.
   */
  [[nodiscard]] VIX_REALTIME_API std::string_view
  to_string(const SessionId &sessionId) noexcept;

} // namespace vix::realtime

namespace std
{
  /**
   * @brief Hash specialization for `vix::realtime::SessionId`.
   */
  template <>
  struct hash<vix::realtime::SessionId>
  {
    /**
     * @brief Compute the hash of a session identifier.
     *
     * @param sessionId Session identifier.
     * @return Hash value.
     */
    [[nodiscard]] std::size_t operator()(
        const vix::realtime::SessionId &sessionId) const noexcept
    {
      return std::hash<std::string_view>{}(sessionId.view());
    }
  };

} // namespace std

#endif // VIX_REALTIME_SESSION_ID_HPP
