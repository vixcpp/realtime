/**
 *
 * @file room_version.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime room version type.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/room_version.hpp>

#include <limits>
#include <string>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  RoomVersion::RoomVersion(value_type value)
      : value_(value)
  {
    if (value < initial_value)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "room version cannot be negative"};
    }
  }

  RoomVersion RoomVersion::next() const
  {
    if (value_ == std::numeric_limits<value_type>::max())
    {
      throw Error{
          ErrorCode::InternalError,
          "room version overflow"};
    }

    return RoomVersion{value_ + 1};
  }

  RoomVersion &RoomVersion::increment()
  {
    *this = next();
    return *this;
  }

  std::string to_string(RoomVersion version)
  {
    return std::to_string(version.value());
  }

} // namespace vix::realtime
