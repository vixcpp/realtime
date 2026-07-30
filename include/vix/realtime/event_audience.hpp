/**
 *
 * @file event_audience.hpp
 * @author Gaspard Kirira
 * @brief Event delivery audience definitions for Vix Realtime.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_EVENT_AUDIENCE_HPP
#define VIX_REALTIME_EVENT_AUDIENCE_HPP

#include <cstdint>
#include <string_view>

#include <vix/realtime/api.hpp>

namespace vix::realtime
{
  /**
   * @brief Defines which sessions may receive a room event.
   *
   * The audience is evaluated by the Realtime runtime before an event is
   * delivered through a transport adapter.
   */
  enum class EventAudience : std::uint8_t
  {
    /**
     * @brief Deliver the event to every authorized session in the room.
     */
    Room = 0,

    /**
     * @brief Deliver the event only to the session that sent the command.
     */
    Sender,

    /**
     * @brief Deliver the event to all authorized room sessions except
     * the session that sent the command.
     */
    Others,

    /**
     * @brief Deliver the event to one explicitly targeted session.
     */
    Session,

    /**
     * @brief Keep the event internal to the server.
     *
     * Internal events may still be persisted and applied to room state,
     * but they are not sent to connected clients.
     */
    Internal
  };

  /**
   * @brief Return the stable textual representation of an event audience.
   *
   * @param audience Event audience value.
   * @return Stable lowercase audience identifier.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(EventAudience audience) noexcept
  {
    switch (audience)
    {
    case EventAudience::Room:
      return "room";

    case EventAudience::Sender:
      return "sender";

    case EventAudience::Others:
      return "others";

    case EventAudience::Session:
      return "session";

    case EventAudience::Internal:
      return "internal";
    }

    return "room";
  }

} // namespace vix::realtime

#endif // VIX_REALTIME_EVENT_AUDIENCE_HPP
