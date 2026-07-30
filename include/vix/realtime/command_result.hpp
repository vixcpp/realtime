/**
 *
 * @file command_result.hpp
 * @author Gaspard Kirira
 * @brief Result produced after handling a Vix Realtime room command.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_COMMAND_RESULT_HPP
#define VIX_REALTIME_COMMAND_RESULT_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <vix/realtime/api.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Status of a room command after handler execution.
   */
  enum class CommandStatus
  {
    /**
     * @brief The command was accepted and may have produced events.
     */
    Accepted = 0,

    /**
     * @brief The command was rejected by application rules.
     */
    Rejected,

    /**
     * @brief The command was ignored without producing events.
     */
    Ignored
  };

  /**
   * @brief Return the stable textual representation of a command status.
   *
   * @param status Command status.
   * @return Stable lowercase status identifier.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(CommandStatus status) noexcept
  {
    switch (status)
    {
    case CommandStatus::Accepted:
      return "accepted";

    case CommandStatus::Rejected:
      return "rejected";

    case CommandStatus::Ignored:
      return "ignored";
    }

    return "rejected";
  }

  /**
   * @brief Represents the outcome of processing one room command.
   *
   * An accepted command may produce zero or more authoritative room events.
   * A rejected command carries a deterministic error code and an optional
   * application-readable message.
   *
   * The room runtime persists and applies events only when the status is
   * `CommandStatus::Accepted`.
   */
  class VIX_REALTIME_API CommandResult
  {
  public:
    /**
     * @brief Construct an accepted result without events.
     */
    CommandResult() = default;

    /**
     * @brief Construct a command result.
     *
     * @param status Result status.
     * @param events Events produced by the command handler.
     * @param errorCode Optional deterministic rejection code.
     * @param message Optional result message.
     */
    CommandResult(
        CommandStatus status,
        std::vector<RoomEvent> events = {},
        std::optional<ErrorCode> errorCode = std::nullopt,
        std::string message = {});

    /**
     * @brief Create an accepted result without events.
     *
     * @return Accepted command result.
     */
    [[nodiscard]] static CommandResult accepted();

    /**
     * @brief Create an accepted result containing one event.
     *
     * @param event Event produced by the command.
     * @return Accepted command result.
     */
    [[nodiscard]] static CommandResult accepted(RoomEvent event);

    /**
     * @brief Create an accepted result containing multiple events.
     *
     * @param events Events produced by the command.
     * @return Accepted command result.
     */
    [[nodiscard]] static CommandResult accepted(
        std::vector<RoomEvent> events);

    /**
     * @brief Create a rejected command result.
     *
     * @param code Deterministic rejection code.
     * @param message Human-readable rejection message.
     * @return Rejected command result.
     */
    [[nodiscard]] static CommandResult rejected(
        ErrorCode code,
        std::string message = {});

    /**
     * @brief Create an ignored command result.
     *
     * Ignored commands produce no events and do not mutate room state.
     *
     * @param message Optional explanation.
     * @return Ignored command result.
     */
    [[nodiscard]] static CommandResult ignored(
        std::string message = {});

    /**
     * @brief Return the result status.
     *
     * @return Command result status.
     */
    [[nodiscard]] CommandStatus status() const noexcept;

    /**
     * @brief Return whether the command was accepted.
     *
     * @return True when the status is `CommandStatus::Accepted`.
     */
    [[nodiscard]] bool is_accepted() const noexcept;

    /**
     * @brief Return whether the command was rejected.
     *
     * @return True when the status is `CommandStatus::Rejected`.
     */
    [[nodiscard]] bool is_rejected() const noexcept;

    /**
     * @brief Return whether the command was ignored.
     *
     * @return True when the status is `CommandStatus::Ignored`.
     */
    [[nodiscard]] bool is_ignored() const noexcept;

    /**
     * @brief Return whether the result contains events.
     *
     * @return True when at least one event is present.
     */
    [[nodiscard]] bool has_events() const noexcept;

    /**
     * @brief Return the number of produced events.
     *
     * @return Event count.
     */
    [[nodiscard]] std::size_t event_count() const noexcept;

    /**
     * @brief Return the produced events.
     *
     * @return Constant reference to the event collection.
     */
    [[nodiscard]] const std::vector<RoomEvent> &
    events() const noexcept;

    /**
     * @brief Return mutable access to the produced events.
     *
     * This is intended for room runtime preparation before persistence.
     *
     * @return Mutable reference to the event collection.
     */
    [[nodiscard]] std::vector<RoomEvent> &events() noexcept;

    /**
     * @brief Return the deterministic error code.
     *
     * The value is present only for rejected results.
     *
     * @return Rejection error code, when available.
     */
    [[nodiscard]] const std::optional<ErrorCode> &
    error_code() const noexcept;

    /**
     * @brief Return the human-readable result message.
     *
     * @return Result message.
     */
    [[nodiscard]] const std::string &message() const noexcept;

    /**
     * @brief Return application-defined result metadata.
     *
     * Metadata may contain diagnostics or protocol response information.
     * It must not contain authoritative room state changes.
     *
     * @return Constant reference to result metadata.
     */
    [[nodiscard]] const JsonObject &metadata() const noexcept;

    /**
     * @brief Add one event to an accepted result.
     *
     * @param event Event to append.
     * @return Current result.
     *
     * @throws vix::realtime::Error
     *         When the result is not accepted.
     */
    CommandResult &add_event(RoomEvent event);

    /**
     * @brief Replace the result message.
     *
     * @param value Result message.
     * @return Current result.
     */
    CommandResult &set_message(std::string value);

    /**
     * @brief Replace application-defined result metadata.
     *
     * @param value Result metadata.
     * @return Current result.
     */
    CommandResult &set_metadata(JsonObject value);

    /**
     * @brief Return whether the result is internally consistent.
     *
     * @return True when status, events, and error code are consistent.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /**
     * @brief Validate the command result.
     *
     * @throws vix::realtime::Error
     *         When status, events, or error information are inconsistent.
     */
    void validate() const;

  private:
    /** @brief Command result status. */
    CommandStatus status_{CommandStatus::Accepted};

    /** @brief Authoritative events produced by an accepted command. */
    std::vector<RoomEvent> events_{};

    /** @brief Deterministic rejection code. */
    std::optional<ErrorCode> errorCode_{};

    /** @brief Human-readable result explanation. */
    std::string message_{};

    /** @brief Non-authoritative result metadata. */
    JsonObject metadata_{};
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_COMMAND_RESULT_HPP
