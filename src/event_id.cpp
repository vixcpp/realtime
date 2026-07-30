/**
 *
 * @file event_id.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime event identifier type.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/event_id.hpp>

#include <limits>
#include <string>

#include <vix/realtime/errors.hpp>

namespace vix::realtime
{
  EventId::EventId(value_type value)
      : value_(value)
  {
    if (value < none_value)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "event identifier cannot be negative"};
    }
  }

  EventId EventId::next() const
  {
    if (value_ == std::numeric_limits<value_type>::max())
    {
      throw Error{
          ErrorCode::InternalError,
          "event identifier overflow"};
    }

    return EventId{value_ + 1};
  }

  EventId &EventId::increment()
  {
    *this = next();
    return *this;
  }

  std::string to_string(EventId eventId)
  {
    return std::to_string(eventId.value());
  }

} // namespace vix::realtime
