/**
 *
 * @file protocol.hpp
 * @author Gaspard Kirira
 * @brief Versioned JSON protocol for the Vix Realtime module.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_PROTOCOL_HPP
#define VIX_REALTIME_PROTOCOL_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <vix/realtime/api.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_snapshot.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime::protocol
{
  /**
   * @brief Current major version of the Realtime wire protocol.
   */
  inline constexpr std::uint32_t version_major = 1;

  /**
   * @brief Current minor version of the Realtime wire protocol.
   */
  inline constexpr std::uint32_t version_minor = 0;

  /**
   * @brief Maximum number of characters allowed in a message type.
   */
  inline constexpr std::size_t max_message_type_size = 128;

  /**
   * @brief Semantic version of the Realtime wire protocol.
   */
  struct Version
  {
    /** @brief Protocol major version. */
    std::uint32_t major{version_major};

    /** @brief Protocol minor version. */
    std::uint32_t minor{version_minor};

    /**
     * @brief Compare two protocol versions.
     */
    auto operator<=>(const Version &) const noexcept = default;
  };

  /**
   * @brief Return whether a protocol version is supported.
   *
   * A version is supported when its major version matches the current
   * protocol and its minor version is not newer than the current one.
   *
   * @param value Protocol version.
   * @return True when the version is supported.
   */
  [[nodiscard]] constexpr bool is_supported(
      Version value) noexcept
  {
    return value.major == version_major &&
           value.minor <= version_minor;
  }

  /**
   * @brief High-level category of a protocol message.
   */
  enum class MessageKind : std::uint8_t
  {
    /** @brief Client request submitted to the server. */
    Request = 0,

    /** @brief Compatibility alias for client room commands. */
    Command = Request,

    /** @brief Server response associated with a request. */
    Response,

    /** @brief Authoritative room event. */
    Event,

    /** @brief Protocol or application error. */
    Error,

    /** @brief Serialized room snapshot. */
    Snapshot,

    /** @brief One-way lifecycle or control message. */
    Control
  };

  /**
   * @brief Return whether a message kind is recognized.
   *
   * @param kind Message kind.
   * @return True when the value represents a known message kind.
   */
  [[nodiscard]] constexpr bool is_valid(
      MessageKind kind) noexcept
  {
    switch (kind)
    {
    case MessageKind::Request:
    case MessageKind::Response:
    case MessageKind::Event:
    case MessageKind::Error:
    case MessageKind::Snapshot:
    case MessageKind::Control:
      return true;
    }

    return false;
  }

  /**
   * @brief Return the stable textual representation of a message kind.
   *
   * @param kind Message kind.
   * @return Stable lowercase identifier.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(MessageKind kind) noexcept
  {
    switch (kind)
    {
    case MessageKind::Request:
      return "request";

    case MessageKind::Response:
      return "response";

    case MessageKind::Event:
      return "event";

    case MessageKind::Error:
      return "error";

    case MessageKind::Snapshot:
      return "snapshot";

    case MessageKind::Control:
      return "control";
    }

    return "unknown";
  }

  /**
   * @brief Parse a textual protocol message kind.
   *
   * @param value Textual message kind.
   * @return Parsed message kind, or no value when unsupported.
   */
  [[nodiscard]] VIX_REALTIME_API std::optional<MessageKind>
  parse_message_kind(std::string_view value) noexcept;

  /**
   * @brief Versioned transport-independent Realtime message envelope.
   *
   * The envelope preserves message identity, correlation, room position,
   * schema version, payload, and metadata independently of the transport.
   */
  class VIX_REALTIME_API Envelope
  {
  public:
    /**
     * @brief Construct an empty protocol envelope.
     *
     * The envelope remains invalid until a message type is assigned.
     */
    Envelope() = default;

    /**
     * @brief Construct a protocol envelope.
     *
     * Only the common fields are validated during construction. Routing fields
     * may be assigned afterward before calling `validate()` or `serialize()`.
     *
     * @param kind High-level message category.
     * @param type Stable protocol or application message type.
     * @param payload Message payload.
     */
    Envelope(
        MessageKind kind,
        std::string type,
        JsonObject payload = {});

    /**
     * @brief Return the protocol version.
     *
     * @return Protocol version.
     */
    [[nodiscard]] Version version() const noexcept;

    /**
     * @brief Return the high-level message category.
     *
     * @return Message kind.
     */
    [[nodiscard]] MessageKind kind() const noexcept;

    /**
     * @brief Return the protocol or application message type.
     *
     * @return Message type.
     */
    [[nodiscard]] const std::string &type() const noexcept;

    /**
     * @brief Return the message payload.
     *
     * @return Constant reference to the payload.
     */
    [[nodiscard]] const JsonObject &payload() const noexcept;

    /**
     * @brief Return mutable access to the message payload.
     *
     * @return Mutable reference to the payload.
     */
    [[nodiscard]] JsonObject &payload() noexcept;

    /**
     * @brief Return the transport-level message identifier.
     *
     * @return Message identifier, or an empty string when absent.
     */
    [[nodiscard]] const std::string &
    message_id() const noexcept;

    /**
     * @brief Return the related client request identifier.
     *
     * @return Request identifier, or an empty string when absent.
     */
    [[nodiscard]] const RequestId &
    request_id() const noexcept;

    /**
     * @brief Return the operation correlation identifier.
     *
     * @return Correlation identifier, or an empty string when absent.
     */
    [[nodiscard]] const CorrelationId &
    correlation_id() const noexcept;

    /**
     * @brief Return the target room identifier.
     *
     * @return Room identifier, when present.
     */
    [[nodiscard]] const std::optional<RoomId> &
    room_id() const noexcept;

    /**
     * @brief Return the logical session identifier.
     *
     * @return Session identifier, when present.
     */
    [[nodiscard]] const std::optional<SessionId> &
    session_id() const noexcept;

    /**
     * @brief Return the room version carried by the message.
     *
     * @return Room version, when present.
     */
    [[nodiscard]] const std::optional<RoomVersion> &
    room_version() const noexcept;

    /**
     * @brief Return the event identifier carried by the message.
     *
     * @return Event identifier, when present.
     */
    [[nodiscard]] const std::optional<EventId> &
    event_id() const noexcept;

    /**
     * @brief Return the application schema version.
     *
     * @return Schema version, when present.
     */
    [[nodiscard]] const std::optional<SchemaVersion> &
    schema_version() const noexcept;

    /**
     * @brief Return the message creation timestamp.
     *
     * @return Message timestamp.
     */
    [[nodiscard]] Timestamp created_at() const noexcept;

    /**
     * @brief Return protocol metadata.
     *
     * @return Constant reference to metadata.
     */
    [[nodiscard]] const JsonObject &metadata() const noexcept;

    /**
     * @brief Set the protocol version.
     *
     * @param value Protocol version.
     * @return Current envelope.
     */
    Envelope &set_version(Version value);

    /**
     * @brief Set the message category.
     *
     * @param value Message kind.
     * @return Current envelope.
     */
    Envelope &set_kind(MessageKind value);

    /**
     * @brief Set the transport-level message identifier.
     *
     * @param value Message identifier.
     * @return Current envelope.
     */
    Envelope &set_message_id(std::string value);

    /**
     * @brief Set the related client request identifier.
     *
     * @param value Request identifier.
     * @return Current envelope.
     */
    Envelope &set_request_id(RequestId value);

    /**
     * @brief Set the operation correlation identifier.
     *
     * @param value Correlation identifier.
     * @return Current envelope.
     */
    Envelope &set_correlation_id(CorrelationId value);

    /**
     * @brief Set the target room identifier.
     *
     * @param value Room identifier.
     * @return Current envelope.
     */
    Envelope &set_room_id(RoomId value);

    /**
     * @brief Remove the target room identifier.
     *
     * @return Current envelope.
     */
    Envelope &clear_room_id() noexcept;

    /**
     * @brief Set the logical session identifier.
     *
     * @param value Session identifier.
     * @return Current envelope.
     */
    Envelope &set_session_id(SessionId value);

    /**
     * @brief Remove the logical session identifier.
     *
     * @return Current envelope.
     */
    Envelope &clear_session_id() noexcept;

    /**
     * @brief Set the room version.
     *
     * @param value Room version.
     * @return Current envelope.
     */
    Envelope &set_room_version(RoomVersion value);

    /**
     * @brief Remove the room version.
     *
     * @return Current envelope.
     */
    Envelope &clear_room_version() noexcept;

    /**
     * @brief Set the event identifier.
     *
     * @param value Event identifier.
     * @return Current envelope.
     */
    Envelope &set_event_id(EventId value);

    /**
     * @brief Remove the event identifier.
     *
     * @return Current envelope.
     */
    Envelope &clear_event_id() noexcept;

    /**
     * @brief Set the application schema version.
     *
     * @param value Non-zero schema version.
     * @return Current envelope.
     */
    Envelope &set_schema_version(SchemaVersion value);

    /**
     * @brief Remove the application schema version.
     *
     * @return Current envelope.
     */
    Envelope &clear_schema_version() noexcept;

    /**
     * @brief Set the message creation timestamp.
     *
     * @param value Message timestamp.
     * @return Current envelope.
     */
    Envelope &set_created_at(Timestamp value) noexcept;

    /**
     * @brief Replace protocol metadata.
     *
     * @param value Protocol metadata.
     * @return Current envelope.
     */
    Envelope &set_metadata(JsonObject value);

    /**
     * @brief Return whether all envelope fields are consistent.
     *
     * @return True when the envelope is valid.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /**
     * @brief Validate all protocol envelope fields.
     *
     * @throws vix::realtime::Error
     *         When the envelope is malformed or uses an unsupported version.
     */
    void validate() const;

    /**
     * @brief Return whether a protocol message type is valid.
     *
     * Valid types contain letters, digits, `_`, `-`, and dot-separated
     * namespaces such as `room.join` or `message.sent`.
     *
     * @param value Candidate message type.
     * @return True when the type is valid.
     */
    [[nodiscard]] static bool is_valid_type(
        std::string_view value) noexcept;

  private:
    /** @brief Protocol version. */
    Version version_{};

    /** @brief High-level message category. */
    MessageKind kind_{MessageKind::Control};

    /** @brief Stable protocol or application message type. */
    std::string type_{};

    /** @brief Structured message payload. */
    JsonObject payload_{};

    /** @brief Unique transport-level message identifier. */
    std::string messageId_{};

    /** @brief Related client request identifier. */
    RequestId requestId_{};

    /** @brief Correlation identifier for related operations. */
    CorrelationId correlationId_{};

    /** @brief Optional room routing identifier. */
    std::optional<RoomId> roomId_{};

    /** @brief Optional logical session identifier. */
    std::optional<SessionId> sessionId_{};

    /** @brief Optional room stream version. */
    std::optional<RoomVersion> roomVersion_{};

    /** @brief Optional persistent event identifier. */
    std::optional<EventId> eventId_{};

    /** @brief Optional application schema version. */
    std::optional<SchemaVersion> schemaVersion_{};

    /** @brief Message creation timestamp. */
    Timestamp createdAt_{SystemClock::now()};

    /** @brief Non-authoritative protocol metadata. */
    JsonObject metadata_{};
  };

  /**
   * @brief Build a request envelope from a room command.
   *
   * @param command Room command.
   * @return Request envelope.
   */
  [[nodiscard]] VIX_REALTIME_API Envelope
  from_command(const RoomCommand &command);

  /**
   * @brief Build an event envelope from a room event.
   *
   * @param event Room event.
   * @return Event envelope.
   */
  [[nodiscard]] VIX_REALTIME_API Envelope
  from_event(const RoomEvent &event);

  /**
   * @brief Build a snapshot envelope from a room snapshot.
   *
   * @param snapshot Room snapshot.
   * @return Snapshot envelope.
   */
  [[nodiscard]] VIX_REALTIME_API Envelope
  from_snapshot(const RoomSnapshot &snapshot);

  /**
   * @brief Build a protocol error envelope.
   *
   * @param code Deterministic error code.
   * @param message Human-readable error message.
   * @param requestId Related request identifier.
   * @param correlationId Related correlation identifier.
   * @return Error envelope.
   */
  [[nodiscard]] VIX_REALTIME_API Envelope
  make_error(
      ErrorCode code,
      std::string message,
      RequestId requestId = {},
      CorrelationId correlationId = {});

  /**
   * @brief Serialize a protocol envelope to compact JSON.
   *
   * @param envelope Envelope to serialize.
   * @return Compact JSON string.
   */
  [[nodiscard]] VIX_REALTIME_API std::string
  serialize(const Envelope &envelope);

  /**
   * @brief Parse and validate a protocol envelope from JSON.
   *
   * @param text Serialized JSON object.
   * @return Parsed protocol envelope.
   *
   * @throws vix::realtime::Error
   *         When the input is malformed or unsupported.
   */
  [[nodiscard]] VIX_REALTIME_API Envelope
  parse(std::string_view text);

} // namespace vix::realtime::protocol

#endif // VIX_REALTIME_PROTOCOL_HPP
