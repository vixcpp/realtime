/**
 *
 * @file room_command.hpp
 * @author Gaspard Kirira
 * @brief Client command representation for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_COMMAND_HPP
#define VIX_REALTIME_ROOM_COMMAND_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <vix/realtime/api.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Represents one client request submitted to a room.
   *
   * Clients submit commands describing intent. Commands do not directly
   * mutate room state. A room handler validates the command and produces
   * zero or more authoritative room events.
   */
  class VIX_REALTIME_API RoomCommand
  {
  public:
    /**
     * @brief Maximum number of characters allowed in a command type.
     */
    static constexpr std::size_t max_type_size = 128;

    /**
     * @brief Construct an empty command.
     *
     * The resulting command is not valid until all required fields are set.
     */
    RoomCommand() = default;

    /**
     * @brief Construct a room command.
     *
     * @param roomId Target room identifier.
     * @param sessionId Logical session submitting the command.
     * @param type Application-defined command type.
     * @param payload Command payload.
     * @param requestId Optional client request identifier used for correlation
     *                  and idempotency.
     */
    RoomCommand(
        RoomId roomId,
        SessionId sessionId,
        std::string type,
        JsonObject payload = {},
        RequestId requestId = {});

    /**
     * @brief Return the target room identifier.
     *
     * @return Target room identifier.
     */
    [[nodiscard]] const RoomId &room_id() const noexcept;

    /**
     * @brief Return the submitting logical session identifier.
     *
     * @return Submitting session identifier.
     */
    [[nodiscard]] const SessionId &session_id() const noexcept;

    /**
     * @brief Return the application-defined command type.
     *
     * @return Command type.
     */
    [[nodiscard]] const std::string &type() const noexcept;

    /**
     * @brief Return the command payload.
     *
     * @return Constant reference to the payload object.
     */
    [[nodiscard]] const JsonObject &payload() const noexcept;

    /**
     * @brief Return mutable access to the command payload.
     *
     * This is intended for protocol construction before the command enters
     * a room queue. A queued command must be treated as immutable.
     *
     * @return Mutable reference to the payload object.
     */
    [[nodiscard]] JsonObject &payload() noexcept;

    /**
     * @brief Return the client request identifier.
     *
     * @return Request identifier, or an empty string when absent.
     */
    [[nodiscard]] const RequestId &request_id() const noexcept;

    /**
     * @brief Return the command correlation identifier.
     *
     * @return Correlation identifier, or an empty string when absent.
     */
    [[nodiscard]] const CorrelationId &correlation_id() const noexcept;

    /**
     * @brief Return the expected room version supplied by the client.
     *
     * When present, the command may be rejected if the current room version
     * differs from this value.
     *
     * @return Expected room version, when supplied.
     */
    [[nodiscard]] const std::optional<RoomVersion> &
    expected_version() const noexcept;

    /**
     * @brief Return the timestamp assigned when the command was created.
     *
     * @return Command creation timestamp.
     */
    [[nodiscard]] Timestamp created_at() const noexcept;

    /**
     * @brief Return application-defined command metadata.
     *
     * Metadata may contain trace information or adapter-specific context.
     * It must not contain authoritative room state.
     *
     * @return Constant reference to command metadata.
     */
    [[nodiscard]] const JsonObject &metadata() const noexcept;

    /**
     * @brief Set the correlation identifier.
     *
     * @param value Correlation identifier.
     * @return Current command.
     */
    RoomCommand &set_correlation_id(CorrelationId value);

    /**
     * @brief Set the expected room version.
     *
     * @param version Expected room version.
     * @return Current command.
     */
    RoomCommand &set_expected_version(RoomVersion version);

    /**
     * @brief Remove the expected room version constraint.
     *
     * @return Current command.
     */
    RoomCommand &clear_expected_version() noexcept;

    /**
     * @brief Set the command creation timestamp.
     *
     * This is primarily used while decoding persisted or protocol data.
     *
     * @param value Command creation timestamp.
     * @return Current command.
     */
    RoomCommand &set_created_at(Timestamp value) noexcept;

    /**
     * @brief Replace application-defined command metadata.
     *
     * @param value Command metadata.
     * @return Current command.
     */
    RoomCommand &set_metadata(JsonObject value);

    /**
     * @brief Return whether the command contains all required valid fields.
     *
     * @return True when the command is valid.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /**
     * @brief Validate the command.
     *
     * @throws vix::realtime::Error
     *         When a required field is missing or invalid.
     */
    void validate() const;

    /**
     * @brief Return whether a command type is valid.
     *
     * Valid command types use dot-separated application names such as:
     *
     * @code
     * object.place
     * citizen.move
     * message.send
     * @endcode
     *
     * @param value Candidate command type.
     * @return True when the command type is valid.
     */
    [[nodiscard]] static bool is_valid_type(
        std::string_view value) noexcept;

  private:
    /** @brief Target room identifier. */
    RoomId roomId_{};

    /** @brief Logical session submitting the command. */
    SessionId sessionId_{};

    /** @brief Application-defined command type. */
    std::string type_{};

    /** @brief Application-defined command payload. */
    JsonObject payload_{};

    /** @brief Client-provided request identifier. */
    RequestId requestId_{};

    /** @brief Identifier correlating commands, events, and diagnostics. */
    CorrelationId correlationId_{};

    /** @brief Optional optimistic room version constraint. */
    std::optional<RoomVersion> expectedVersion_{};

    /** @brief Time at which the command was created. */
    Timestamp createdAt_{SystemClock::now()};

    /** @brief Non-authoritative command metadata. */
    JsonObject metadata_{};
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_ROOM_COMMAND_HPP
