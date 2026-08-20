#include <vix/realtime/errors.hpp>

#include <utility>

namespace vix::realtime
{
  std::string_view to_string(ErrorCode code) noexcept
  {
    switch (code)
    {
    case ErrorCode::None:
      return "none";
    case ErrorCode::InvalidConfiguration:
      return "invalid_configuration";
    case ErrorCode::MissingDependency:
      return "missing_dependency";
    case ErrorCode::RoomNotFound:
      return "room_not_found";
    case ErrorCode::RoomAlreadyExists:
      return "room_already_exists";
    case ErrorCode::RoomFull:
      return "room_full";
    case ErrorCode::RoomLimitReached:
      return "room_limit_reached";
    case ErrorCode::RoomNotReady:
      return "room_not_ready";
    case ErrorCode::RoomClosed:
      return "room_closed";
    case ErrorCode::CommandQueueFull:
      return "command_queue_full";
    case ErrorCode::InvalidCommand:
      return "invalid_command";
    case ErrorCode::CommandRejected:
      return "command_rejected";
    case ErrorCode::CommandTimeout:
      return "command_timeout";
    case ErrorCode::Unauthorized:
      return "unauthorized";
    case ErrorCode::SessionNotFound:
      return "session_not_found";
    case ErrorCode::SessionExpired:
      return "session_expired";
    case ErrorCode::InvalidResumeToken:
      return "invalid_resume_token";
    case ErrorCode::ConnectionNotAttached:
      return "connection_not_attached";
    case ErrorCode::MembershipNotFound:
      return "membership_not_found";
    case ErrorCode::AlreadyJoined:
      return "already_joined";
    case ErrorCode::InvalidProtocolMessage:
      return "invalid_protocol_message";
    case ErrorCode::UnsupportedProtocolVersion:
      return "unsupported_protocol_version";
    case ErrorCode::PayloadTooLarge:
      return "payload_too_large";
    case ErrorCode::EventStoreFailure:
      return "event_store_failure";
    case ErrorCode::SnapshotStoreFailure:
      return "snapshot_store_failure";
    case ErrorCode::CorruptedState:
      return "corrupted_state";
    case ErrorCode::EventApplyFailure:
      return "event_apply_failure";
    case ErrorCode::ReplayUnavailable:
      return "replay_unavailable";
    case ErrorCode::ReplayLimitExceeded:
      return "replay_limit_exceeded";
    case ErrorCode::TransportFailure:
      return "transport_failure";
    case ErrorCode::Cancelled:
      return "cancelled";
    case ErrorCode::Timeout:
      return "timeout";
    case ErrorCode::InternalError:
      return "internal_error";
    case ErrorCode::SessionAlreadyConnected:
      return "session_already_connected";
    case ErrorCode::SessionNotDetached:
      return "session_not_detached";
    }

    return "internal_error";
  }

  Error::Error(
      ErrorCode code,
      std::string message)
      : std::runtime_error(std::move(message)),
        code_(code)
  {
  }

  ErrorCode Error::code() const noexcept
  {
    return code_;
  }
} // namespace vix::realtime
