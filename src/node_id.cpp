/**
 *
 * @file node_id.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime runtime node identifier.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/node_id.hpp>

#include <cctype>
#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  namespace
  {
    /**
     * @brief Return whether a character is an accepted separator.
     */
    [[nodiscard]] bool is_separator(char value) noexcept
    {
      return value == '-' ||
             value == '_' ||
             value == '.' ||
             value == ':';
    }

    /**
     * @brief Return whether a character is allowed in a node identifier.
     */
    [[nodiscard]] bool is_allowed_character(char value) noexcept
    {
      const auto character = static_cast<unsigned char>(value);

      return std::isalnum(character) != 0 || is_separator(value);
    }

  } // namespace

  NodeId::NodeId(std::string value)
      : value_(std::move(value))
  {
    validate(value_);
  }

  NodeId::NodeId(std::string_view value)
      : NodeId(std::string{value})
  {
  }

  const std::string &NodeId::value() const noexcept
  {
    return value_;
  }

  std::string_view NodeId::view() const noexcept
  {
    return value_;
  }

  bool NodeId::empty() const noexcept
  {
    return value_.empty();
  }

  std::size_t NodeId::size() const noexcept
  {
    return value_.size();
  }

  bool NodeId::is_valid(std::string_view value) noexcept
  {
    if (value.empty() || value.size() > max_size)
    {
      return false;
    }

    if (is_separator(value.front()) || is_separator(value.back()))
    {
      return false;
    }

    bool previousWasSeparator = false;

    for (const char character : value)
    {
      if (!is_allowed_character(character))
      {
        return false;
      }

      const bool currentIsSeparator = is_separator(character);

      if (currentIsSeparator && previousWasSeparator)
      {
        return false;
      }

      previousWasSeparator = currentIsSeparator;
    }

    return true;
  }

  void NodeId::validate(std::string_view value)
  {
    if (value.empty())
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "node identifier cannot be empty"};
    }

    if (value.size() > max_size)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "node identifier exceeds the maximum size of 128 characters"};
    }

    if (is_separator(value.front()))
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "node identifier cannot begin with a separator"};
    }

    if (is_separator(value.back()))
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "node identifier cannot end with a separator"};
    }

    bool previousWasSeparator = false;

    for (const char character : value)
    {
      if (!is_allowed_character(character))
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "node identifier contains an unsupported character"};
      }

      const bool currentIsSeparator = is_separator(character);

      if (currentIsSeparator && previousWasSeparator)
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "node identifier cannot contain consecutive separators"};
      }

      previousWasSeparator = currentIsSeparator;
    }
  }

  std::string_view to_string(const NodeId &nodeId) noexcept
  {
    return nodeId.view();
  }

} // namespace vix::realtime
