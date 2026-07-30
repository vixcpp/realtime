/**
 *
 * @file session_id.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime logical session identifier.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/session_id.hpp>

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
      return value == '-' || value == '_' || value == '.';
    }

    /**
     * @brief Return whether a character is allowed in a session identifier.
     */
    [[nodiscard]] bool is_allowed_character(char value) noexcept
    {
      const auto character = static_cast<unsigned char>(value);

      return std::isalnum(character) != 0 || is_separator(value);
    }

  } // namespace

  SessionId::SessionId(std::string value)
      : value_(std::move(value))
  {
    validate(value_);
  }

  SessionId::SessionId(std::string_view value)
      : SessionId(std::string{value})
  {
  }

  const std::string &SessionId::value() const noexcept
  {
    return value_;
  }

  std::string_view SessionId::view() const noexcept
  {
    return value_;
  }

  bool SessionId::empty() const noexcept
  {
    return value_.empty();
  }

  std::size_t SessionId::size() const noexcept
  {
    return value_.size();
  }

  bool SessionId::is_valid(std::string_view value) noexcept
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

  void SessionId::validate(std::string_view value)
  {
    if (value.empty())
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "session identifier cannot be empty"};
    }

    if (value.size() > max_size)
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "session identifier exceeds the maximum size of 128 characters"};
    }

    if (is_separator(value.front()))
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "session identifier cannot begin with a separator"};
    }

    if (is_separator(value.back()))
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "session identifier cannot end with a separator"};
    }

    bool previousWasSeparator = false;

    for (const char character : value)
    {
      if (!is_allowed_character(character))
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "session identifier contains an unsupported character"};
      }

      const bool currentIsSeparator = is_separator(character);

      if (currentIsSeparator && previousWasSeparator)
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "session identifier cannot contain consecutive separators"};
      }

      previousWasSeparator = currentIsSeparator;
    }
  }

  std::string_view to_string(const SessionId &sessionId) noexcept
  {
    return sessionId.view();
  }

} // namespace vix::realtime
