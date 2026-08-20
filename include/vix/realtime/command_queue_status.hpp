/**
 * @file command_queue_status.hpp
 * @brief Results returned when a room command is queued.
 */

#ifndef VIX_REALTIME_COMMAND_QUEUE_STATUS_HPP
#define VIX_REALTIME_COMMAND_QUEUE_STATUS_HPP

#include <cstdint>
#include <string_view>

#include <vix/realtime/api.hpp>

namespace vix::realtime
{
  /** @brief Result of attempting to enqueue a room command. */
  enum class CommandQueueStatus : std::uint8_t
  {
    Success = 0,
    Full,
    Empty,
    Closed,
    Timeout
  };

  [[nodiscard]] constexpr std::string_view
  to_string(CommandQueueStatus status) noexcept
  {
    switch (status)
    {
    case CommandQueueStatus::Success:
      return "success";
    case CommandQueueStatus::Full:
      return "full";
    case CommandQueueStatus::Empty:
      return "empty";
    case CommandQueueStatus::Closed:
      return "closed";
    case CommandQueueStatus::Timeout:
      return "timeout";
    }

    return "closed";
  }
} // namespace vix::realtime

#endif // VIX_REALTIME_COMMAND_QUEUE_STATUS_HPP
