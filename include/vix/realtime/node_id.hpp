/**
 *
 * @file node_id.hpp
 * @author Gaspard Kirira
 * @brief Strong identifier type for Vix Realtime runtime nodes.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_NODE_ID_HPP
#define VIX_REALTIME_NODE_ID_HPP

#include <compare>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include <vix/realtime/api.hpp>

namespace vix::realtime
{
  /**
   * @brief Strong identifier for a Realtime runtime node.
   *
   * A node represents one process participating in room ownership or routing.
   * The V1 runs locally in one process, but the identifier is introduced early
   * to keep room directory and ownership contracts ready for future clustering.
   *
   * Valid node identifiers:
   *
   * - are not empty;
   * - contain at most `max_size` characters;
   * - contain only ASCII letters, digits, `_`, `-`, `.`, or `:`;
   * - do not begin or end with a separator;
   * - do not contain consecutive separators.
   */
  class VIX_REALTIME_API NodeId
  {
  public:
    /**
     * @brief Maximum number of characters allowed in a node identifier.
     */
    static constexpr std::size_t max_size = 128;

    /**
     * @brief Construct an empty node identifier.
     *
     * The empty value represents an uninitialized runtime node.
     */
    NodeId() = default;

    /**
     * @brief Construct and validate a node identifier.
     *
     * @param value Node identifier value.
     *
     * @throws vix::realtime::Error
     *         When the supplied identifier is invalid.
     */
    explicit NodeId(std::string value);

    /**
     * @brief Construct and validate a node identifier from a string view.
     *
     * @param value Node identifier value.
     *
     * @throws vix::realtime::Error
     *         When the supplied identifier is invalid.
     */
    explicit NodeId(std::string_view value);

    /**
     * @brief Return the stored identifier.
     *
     * @return Constant reference to the node identifier string.
     */
    [[nodiscard]] const std::string &value() const noexcept;

    /**
     * @brief Return a non-owning view of the identifier.
     *
     * @return String view of the node identifier.
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
     * @brief Return whether a string is a valid node identifier.
     *
     * @param value Candidate identifier.
     * @return True when the candidate is valid.
     */
    [[nodiscard]] static bool is_valid(std::string_view value) noexcept;

    /**
     * @brief Validate a node identifier.
     *
     * @param value Candidate identifier.
     *
     * @throws vix::realtime::Error
     *         When the candidate is invalid.
     */
    static void validate(std::string_view value);

    /**
     * @brief Compare two node identifiers.
     */
    auto operator<=>(const NodeId &) const noexcept = default;

  private:
    /** @brief Validated runtime node identifier. */
    std::string value_{};
  };

  /**
   * @brief Return the textual value of a node identifier.
   *
   * @param nodeId Node identifier.
   * @return String view referencing the identifier value.
   */
  [[nodiscard]] VIX_REALTIME_API std::string_view
  to_string(const NodeId &nodeId) noexcept;

} // namespace vix::realtime

namespace std
{
  /**
   * @brief Hash specialization for `vix::realtime::NodeId`.
   */
  template <>
  struct hash<vix::realtime::NodeId>
  {
    /**
     * @brief Compute the hash of a node identifier.
     *
     * @param nodeId Node identifier.
     * @return Hash value.
     */
    [[nodiscard]] std::size_t operator()(
        const vix::realtime::NodeId &nodeId) const noexcept
    {
      return std::hash<std::string_view>{}(nodeId.view());
    }
  };

} // namespace std

#endif // VIX_REALTIME_NODE_ID_HPP
