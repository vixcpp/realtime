/**
 *
 * @file protocol.cpp
 * @author Gaspard Kirira
 * @brief Implementation of the Vix Realtime JSON protocol.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/protocol.hpp>

#include <cctype>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include <vix/json/json.hpp>

namespace vix::realtime::protocol
{
  namespace
  {
    using Json = vix::json::Json;

    /**
     * @brief Convert a Vix Simple object to the full JSON representation.
     */
    [[nodiscard]] Json simple_object_to_json(
        const JsonObject &value)
    {
      return vix::json::to_json(value);
    }

    /**
     * @brief Convert a full JSON value to a recursive Vix Simple token.
     */
    [[nodiscard]] vix::json::token json_to_simple_token(
        const Json &value)
    {
      if (value.is_null())
      {
        return vix::json::token{nullptr};
      }

      if (value.is_boolean())
      {
        return vix::json::token{
            value.get<bool>()};
      }

      if (value.is_number_integer())
      {
        return vix::json::token{
            value.get<std::int64_t>()};
      }

      if (value.is_number_unsigned())
      {
        const auto number =
            value.get<std::uint64_t>();

        if (number >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()))
        {
          throw Error{
              ErrorCode::InvalidProtocolMessage,
              "realtime protocol integer exceeds int64 range"};
        }

        return vix::json::token{
            static_cast<std::int64_t>(number)};
      }

      if (value.is_number_float())
      {
        return vix::json::token{
            value.get<double>()};
      }

      if (value.is_string())
      {
        return vix::json::token{
            value.get<std::string>()};
      }

      if (value.is_array())
      {
        vix::json::array_t array;
        array.reserve(value.size());

        for (const auto &element : value)
        {
          array.push_back(
              json_to_simple_token(element));
        }

        return vix::json::token{array};
      }

      if (value.is_object())
      {
        vix::json::kvs object;
        object.reserve_pairs(value.size());

        for (auto iterator = value.begin();
             iterator != value.end();
             ++iterator)
        {
          object.set(
              iterator.key(),
              json_to_simple_token(iterator.value()));
        }

        return vix::json::token{object};
      }

      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol contains an unsupported JSON value"};
    }

    /**
     * @brief Convert a full JSON object to a Vix Simple object.
     */
    [[nodiscard]] JsonObject json_to_simple_object(
        const Json &value)
    {
      if (!value.is_object())
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime protocol payload must be a JSON object"};
      }

      JsonObject result;
      result.reserve_pairs(value.size());

      for (auto iterator = value.begin();
           iterator != value.end();
           ++iterator)
      {
        result.set(
            iterator.key(),
            json_to_simple_token(iterator.value()));
      }

      return result;
    }

    /**
     * @brief Convert a system timestamp to Unix milliseconds.
     */
    [[nodiscard]] std::int64_t timestamp_to_milliseconds(
        Timestamp value) noexcept
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(
                 value.time_since_epoch())
          .count();
    }

    /**
     * @brief Convert Unix milliseconds to a system timestamp.
     */
    [[nodiscard]] Timestamp timestamp_from_milliseconds(
        std::int64_t value) noexcept
    {
      return Timestamp{
          std::chrono::milliseconds{value}};
    }

    /**
     * @brief Read an optional string field.
     */
    [[nodiscard]] std::string get_optional_string(
        const Json &object,
        std::string_view key)
    {
      const auto iterator =
          object.find(std::string{key});

      if (iterator == object.end() ||
          iterator->is_null())
      {
        return {};
      }

      if (!iterator->is_string())
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime protocol field '" +
                std::string{key} +
                "' must be a string"};
      }

      return iterator->get<std::string>();
    }

    /**
     * @brief Read a required string field.
     */
    [[nodiscard]] std::string get_required_string(
        const Json &object,
        std::string_view key)
    {
      const auto value =
          get_optional_string(object, key);

      if (value.empty())
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime protocol field '" +
                std::string{key} +
                "' is required"};
      }

      return value;
    }

    /**
     * @brief Read an optional integer field.
     */
    template <typename Integer>
    [[nodiscard]] std::optional<Integer>
    get_optional_integer(
        const Json &object,
        std::string_view key)
    {
      const auto iterator =
          object.find(std::string{key});

      if (iterator == object.end() ||
          iterator->is_null())
      {
        return std::nullopt;
      }

      if (!iterator->is_number_integer() &&
          !iterator->is_number_unsigned())
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime protocol field '" +
                std::string{key} +
                "' must be an integer"};
      }

      try
      {
        return iterator->get<Integer>();
      }
      catch (const nlohmann::json::exception &)
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime protocol field '" +
                std::string{key} +
                "' is outside the supported range"};
      }
    }

    /**
     * @brief Append all common envelope fields to a JSON object.
     */
    void add_common_fields(
        Json &json,
        const Envelope &envelope)
    {
      Json protocolVersion = Json::object();
      protocolVersion["major"] =
          envelope.version().major;
      protocolVersion["minor"] =
          envelope.version().minor;

      json["protocol"] =
          std::move(protocolVersion);

      json["kind"] =
          std::string{to_string(envelope.kind())};

      json["type"] =
          envelope.type();

      json["payload"] =
          simple_object_to_json(envelope.payload());

      json["created_at"] =
          timestamp_to_milliseconds(
              envelope.created_at());

      if (!envelope.message_id().empty())
      {
        json["message_id"] =
            envelope.message_id();
      }

      if (!envelope.request_id().empty())
      {
        json["request_id"] =
            envelope.request_id();
      }

      if (!envelope.correlation_id().empty())
      {
        json["correlation_id"] =
            envelope.correlation_id();
      }

      if (envelope.room_id())
      {
        json["room_id"] =
            std::string{
                envelope.room_id()->view()};
      }

      if (envelope.session_id())
      {
        json["session_id"] =
            std::string{
                envelope.session_id()->view()};
      }

      if (envelope.room_version())
      {
        json["room_version"] =
            envelope.room_version()->value();
      }

      if (envelope.event_id())
      {
        json["event_id"] =
            envelope.event_id()->value();
      }

      if (envelope.schema_version())
      {
        json["schema_version"] =
            *envelope.schema_version();
      }

      if (!envelope.metadata().empty())
      {
        json["metadata"] =
            simple_object_to_json(
                envelope.metadata());
      }
    }

  } // namespace

  std::optional<MessageKind>
  parse_message_kind(std::string_view value) noexcept
  {
    if (value == "request")
    {
      return MessageKind::Request;
    }

    if (value == "response")
    {
      return MessageKind::Response;
    }

    if (value == "event")
    {
      return MessageKind::Event;
    }

    if (value == "error")
    {
      return MessageKind::Error;
    }

    if (value == "snapshot")
    {
      return MessageKind::Snapshot;
    }

    if (value == "control")
    {
      return MessageKind::Control;
    }

    return std::nullopt;
  }

  Envelope::Envelope(
      MessageKind kind,
      std::string type,
      JsonObject payload)
      : version_(),
        kind_(kind),
        type_(std::move(type)),
        payload_(std::move(payload)),
        messageId_(),
        requestId_(),
        correlationId_(),
        roomId_(),
        sessionId_(),
        roomVersion_(),
        eventId_(),
        schemaVersion_(),
        createdAt_(SystemClock::now()),
        metadata_()
  {
    if (!protocol::is_valid(kind_))
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol message kind is invalid"};
    }

    if (!is_valid_type(type_))
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol message type is invalid"};
    }
  }

  Version Envelope::version() const noexcept
  {
    return version_;
  }

  MessageKind Envelope::kind() const noexcept
  {
    return kind_;
  }

  const std::string &Envelope::type() const noexcept
  {
    return type_;
  }

  const JsonObject &Envelope::payload() const noexcept
  {
    return payload_;
  }

  JsonObject &Envelope::payload() noexcept
  {
    return payload_;
  }

  const std::string &
  Envelope::message_id() const noexcept
  {
    return messageId_;
  }

  const RequestId &
  Envelope::request_id() const noexcept
  {
    return requestId_;
  }

  const CorrelationId &
  Envelope::correlation_id() const noexcept
  {
    return correlationId_;
  }

  const std::optional<RoomId> &
  Envelope::room_id() const noexcept
  {
    return roomId_;
  }

  const std::optional<SessionId> &
  Envelope::session_id() const noexcept
  {
    return sessionId_;
  }

  const std::optional<RoomVersion> &
  Envelope::room_version() const noexcept
  {
    return roomVersion_;
  }

  const std::optional<EventId> &
  Envelope::event_id() const noexcept
  {
    return eventId_;
  }

  const std::optional<SchemaVersion> &
  Envelope::schema_version() const noexcept
  {
    return schemaVersion_;
  }

  Timestamp Envelope::created_at() const noexcept
  {
    return createdAt_;
  }

  const JsonObject &
  Envelope::metadata() const noexcept
  {
    return metadata_;
  }

  Envelope &Envelope::set_version(Version value)
  {
    if (value.major == 0)
    {
      throw Error{
          ErrorCode::UnsupportedProtocolVersion,
          "realtime protocol major version cannot be zero"};
    }

    version_ = value;
    return *this;
  }

  Envelope &Envelope::set_kind(MessageKind value)
  {
    if (!protocol::is_valid(value))
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol message kind is invalid"};
    }

    kind_ = value;
    return *this;
  }

  Envelope &Envelope::set_message_id(
      std::string value)
  {
    messageId_ = std::move(value);
    return *this;
  }

  Envelope &Envelope::set_request_id(
      RequestId value)
  {
    requestId_ = std::move(value);
    return *this;
  }

  Envelope &Envelope::set_correlation_id(
      CorrelationId value)
  {
    correlationId_ = std::move(value);
    return *this;
  }

  Envelope &Envelope::set_room_id(RoomId value)
  {
    roomId_ = std::move(value);
    return *this;
  }

  Envelope &Envelope::clear_room_id() noexcept
  {
    roomId_.reset();
    return *this;
  }

  Envelope &Envelope::set_session_id(
      SessionId value)
  {
    sessionId_ = std::move(value);
    return *this;
  }

  Envelope &Envelope::clear_session_id() noexcept
  {
    sessionId_.reset();
    return *this;
  }

  Envelope &Envelope::set_room_version(
      RoomVersion value)
  {
    roomVersion_ = value;
    return *this;
  }

  Envelope &Envelope::clear_room_version() noexcept
  {
    roomVersion_.reset();
    return *this;
  }

  Envelope &Envelope::set_event_id(EventId value)
  {
    eventId_ = value;
    return *this;
  }

  Envelope &Envelope::clear_event_id() noexcept
  {
    eventId_.reset();
    return *this;
  }

  Envelope &Envelope::set_schema_version(
      SchemaVersion value)
  {
    if (value == 0)
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol schema version cannot be zero"};
    }

    schemaVersion_ = value;
    return *this;
  }

  Envelope &
  Envelope::clear_schema_version() noexcept
  {
    schemaVersion_.reset();
    return *this;
  }

  Envelope &Envelope::set_created_at(
      Timestamp value) noexcept
  {
    createdAt_ = value;
    return *this;
  }

  Envelope &Envelope::set_metadata(
      JsonObject value)
  {
    metadata_ = std::move(value);
    return *this;
  }

  bool Envelope::is_valid() const noexcept
  {
    if (!is_supported(version_) ||
        !protocol::is_valid(kind_) ||
        !is_valid_type(type_))
    {
      return false;
    }

    if (schemaVersion_.has_value() &&
        *schemaVersion_ == 0)
    {
      return false;
    }

    if (kind_ == MessageKind::Event)
    {
      return roomId_.has_value() &&
             roomVersion_.has_value() &&
             eventId_.has_value() &&
             !eventId_->empty() &&
             schemaVersion_.has_value();
    }

    if (kind_ == MessageKind::Snapshot)
    {
      return roomId_.has_value() &&
             roomVersion_.has_value() &&
             eventId_.has_value() &&
             !eventId_->empty() &&
             schemaVersion_.has_value();
    }

    if (kind_ == MessageKind::Request)
    {
      return roomId_.has_value() &&
             sessionId_.has_value();
    }

    return true;
  }

  void Envelope::validate() const
  {
    if (!is_supported(version_))
    {
      throw Error{
          ErrorCode::UnsupportedProtocolVersion,
          "unsupported realtime protocol version"};
    }

    if (!protocol::is_valid(kind_))
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol message kind is invalid"};
    }

    if (!is_valid_type(type_))
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol message type is invalid"};
    }

    if (schemaVersion_.has_value() &&
        *schemaVersion_ == 0)
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol schema version cannot be zero"};
    }

    if (kind_ == MessageKind::Event)
    {
      if (!roomId_)
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime event envelope requires a room identifier"};
      }

      if (!roomVersion_)
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime event envelope requires a room version"};
      }

      if (!eventId_ || eventId_->empty())
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime event envelope requires an event identifier"};
      }

      if (!schemaVersion_)
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime event envelope requires a schema version"};
      }
    }

    if (kind_ == MessageKind::Snapshot)
    {
      if (!roomId_)
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime snapshot envelope requires a room identifier"};
      }

      if (!roomVersion_)
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime snapshot envelope requires a room version"};
      }

      if (!eventId_ || eventId_->empty())
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime snapshot envelope requires an event identifier"};
      }

      if (!schemaVersion_)
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime snapshot envelope requires a schema version"};
      }
    }

    if (kind_ == MessageKind::Request)
    {
      if (!roomId_)
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime request envelope requires a room identifier"};
      }

      if (!sessionId_)
      {
        throw Error{
            ErrorCode::InvalidProtocolMessage,
            "realtime request envelope requires a session identifier"};
      }
    }
  }

  bool Envelope::is_valid_type(
      std::string_view value) noexcept
  {
    if (value.empty() ||
        value.size() > max_message_type_size)
    {
      return false;
    }

    if (value.front() == '.' ||
        value.back() == '.')
    {
      return false;
    }

    bool previousWasDot = false;

    for (const char character : value)
    {
      const auto byte =
          static_cast<unsigned char>(character);

      const bool alphaNumeric =
          std::isalnum(byte) != 0;

      const bool allowedSeparator =
          character == '.' ||
          character == '-' ||
          character == '_';

      if (!alphaNumeric &&
          !allowedSeparator)
      {
        return false;
      }

      const bool currentIsDot =
          character == '.';

      if (currentIsDot && previousWasDot)
      {
        return false;
      }

      previousWasDot = currentIsDot;
    }

    return true;
  }

  Envelope from_command(const RoomCommand &command)
  {
    command.validate();

    Envelope envelope{
        MessageKind::Request,
        command.type(),
        command.payload()};

    envelope
        .set_room_id(command.room_id())
        .set_session_id(command.session_id())
        .set_created_at(command.created_at())
        .set_metadata(command.metadata());

    if (!command.request_id().empty())
    {
      envelope.set_request_id(
          command.request_id());
    }

    if (!command.correlation_id().empty())
    {
      envelope.set_correlation_id(
          command.correlation_id());
    }

    if (command.expected_version())
    {
      envelope.set_room_version(
          *command.expected_version());
    }

    return envelope;
  }

  Envelope from_event(const RoomEvent &event)
  {
    event.validate();

    Envelope envelope{
        MessageKind::Event,
        event.type(),
        event.payload()};

    envelope
        .set_room_id(event.room_id())
        .set_room_version(event.room_version())
        .set_event_id(event.event_id())
        .set_schema_version(event.schema_version())
        .set_created_at(event.created_at())
        .set_metadata(event.metadata());

    if (event.source_session())
    {
      envelope.set_session_id(
          *event.source_session());
    }

    if (!event.request_id().empty())
    {
      envelope.set_request_id(
          event.request_id());
    }

    if (!event.correlation_id().empty())
    {
      envelope.set_correlation_id(
          event.correlation_id());
    }

    return envelope;
  }

  Envelope from_snapshot(
      const RoomSnapshot &snapshot)
  {
    snapshot.validate();

    Envelope envelope{
        MessageKind::Snapshot,
        "room.snapshot",
        snapshot.state()};

    envelope
        .set_room_id(snapshot.room_id())
        .set_room_version(
            snapshot.room_version())
        .set_event_id(
            snapshot.last_event_id())
        .set_schema_version(
            snapshot.schema_version())
        .set_created_at(
            snapshot.created_at())
        .set_metadata(
            snapshot.metadata());

    return envelope;
  }

  Envelope make_error(
      ErrorCode code,
      std::string message,
      RequestId requestId,
      CorrelationId correlationId)
  {
    if (code == ErrorCode::None)
    {
      code = ErrorCode::InternalError;
    }

    JsonObject payload;
    payload.set_string(
        "code",
        std::string{
            vix::realtime::to_string(code)});

    payload.set_string(
        "message",
        std::move(message));

    Envelope envelope{
        MessageKind::Error,
        "error",
        std::move(payload)};

    if (!requestId.empty())
    {
      envelope.set_request_id(
          std::move(requestId));
    }

    if (!correlationId.empty())
    {
      envelope.set_correlation_id(
          std::move(correlationId));
    }

    return envelope;
  }

  std::string serialize(const Envelope &envelope)
  {
    envelope.validate();

    Json json = Json::object();
    add_common_fields(json, envelope);

    return vix::json::dumps_compact(json);
  }

  Envelope parse(std::string_view text)
  {
    Json json;

    try
    {
      json = vix::json::loads(text);
    }
    catch (const nlohmann::json::exception &)
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "invalid realtime protocol JSON"};
    }

    if (!json.is_object())
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol message must be a JSON object"};
    }

    const auto protocolIterator =
        json.find("protocol");

    if (protocolIterator == json.end() ||
        !protocolIterator->is_object())
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol version is missing"};
    }

    const auto major =
        get_optional_integer<std::uint32_t>(
            *protocolIterator,
            "major");

    const auto minor =
        get_optional_integer<std::uint32_t>(
            *protocolIterator,
            "minor");

    if (!major || !minor)
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol version is incomplete"};
    }

    const Version version{
        *major,
        *minor};

    if (!is_supported(version))
    {
      throw Error{
          ErrorCode::UnsupportedProtocolVersion,
          "unsupported realtime protocol version"};
    }

    const std::string kindText =
        get_required_string(json, "kind");

    const auto kind =
        parse_message_kind(kindText);

    if (!kind)
    {
      throw Error{
          ErrorCode::InvalidProtocolMessage,
          "realtime protocol message kind is invalid"};
    }

    const std::string type =
        get_required_string(json, "type");

    JsonObject payload;

    const auto payloadIterator =
        json.find("payload");

    if (payloadIterator != json.end() &&
        !payloadIterator->is_null())
    {
      payload =
          json_to_simple_object(
              *payloadIterator);
    }

    Envelope envelope{
        *kind,
        type,
        std::move(payload)};

    envelope.set_version(version);

    const std::string messageId =
        get_optional_string(
            json,
            "message_id");

    if (!messageId.empty())
    {
      envelope.set_message_id(
          messageId);
    }

    const std::string requestId =
        get_optional_string(
            json,
            "request_id");

    if (!requestId.empty())
    {
      envelope.set_request_id(
          requestId);
    }

    const std::string correlationId =
        get_optional_string(
            json,
            "correlation_id");

    if (!correlationId.empty())
    {
      envelope.set_correlation_id(
          correlationId);
    }

    const std::string roomId =
        get_optional_string(
            json,
            "room_id");

    if (!roomId.empty())
    {
      envelope.set_room_id(
          RoomId{roomId});
    }

    const std::string sessionId =
        get_optional_string(
            json,
            "session_id");

    if (!sessionId.empty())
    {
      envelope.set_session_id(
          SessionId{sessionId});
    }

    const auto roomVersion =
        get_optional_integer<VersionValue>(
            json,
            "room_version");

    if (roomVersion)
    {
      envelope.set_room_version(
          RoomVersion{*roomVersion});
    }

    const auto eventId =
        get_optional_integer<EventIdValue>(
            json,
            "event_id");

    if (eventId)
    {
      envelope.set_event_id(
          EventId{*eventId});
    }

    const auto schemaVersion =
        get_optional_integer<SchemaVersion>(
            json,
            "schema_version");

    if (schemaVersion)
    {
      envelope.set_schema_version(
          *schemaVersion);
    }

    const auto createdAt =
        get_optional_integer<std::int64_t>(
            json,
            "created_at");

    if (createdAt)
    {
      envelope.set_created_at(
          timestamp_from_milliseconds(
              *createdAt));
    }

    const auto metadataIterator =
        json.find("metadata");

    if (metadataIterator != json.end() &&
        !metadataIterator->is_null())
    {
      envelope.set_metadata(
          json_to_simple_object(
              *metadataIterator));
    }

    envelope.validate();
    return envelope;
  }

} // namespace vix::realtime::protocol
