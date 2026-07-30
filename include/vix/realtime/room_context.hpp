/**
 *
 * @file room_context.hpp
 * @author Gaspard Kirira
 * @brief Immutable execution context for Vix Realtime room commands.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_CONTEXT_HPP
#define VIX_REALTIME_ROOM_CONTEXT_HPP

#include <optional>

#include <vix/realtime/api.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Immutable runtime information supplied to a room handler.
   *
   * The context describes the room position before a command is processed.
   * It allows handlers to make deterministic decisions without accessing the
   * room runtime, persistence stores, transport, or global mutable state.
   */
  class VIX_REALTIME_API RoomContext
  {
  public:
    /**
     * @brief Construct an empty room context.
     *
     * The resulting context is invalid until a room identifier is assigned
     * through a complete constructor.
     */
    RoomContext() = default;

    /**
     * @brief Construct a room execution context.
     *
     * @param roomId Room being processed.
     * @param roomVersion Current room version before command execution.
     * @param lastEventId Last persisted event in the room stream.
     * @param sessionId Logical session submitting the command.
     * @param requestId Related client request identifier.
     * @param correlationId Operation correlation identifier.
     * @param nodeId Runtime node currently owning the room.
     * @param now Timestamp assigned by the runtime.
     * @param metadata Runtime-defined contextual metadata.
     */
    RoomContext(
        RoomId roomId,
        RoomVersion roomVersion,
        EventId lastEventId,
        std::optional<SessionId> sessionId = std::nullopt,
        RequestId requestId = {},
        CorrelationId correlationId = {},
        std::optional<NodeId> nodeId = std::nullopt,
        Timestamp now = SystemClock::now(),
        JsonObject metadata = {});

    /**
     * @brief Build an execution context from a room command.
     *
     * The room, session, request, and correlation identifiers are copied from
     * the command. The supplied versions describe the room before handling it.
     *
     * @param command Command being processed.
     * @param roomVersion Current room version.
     * @param lastEventId Last persisted event identifier.
     * @param nodeId Runtime node currently owning the room.
     * @param now Timestamp assigned by the runtime.
     * @param metadata Runtime-defined contextual metadata.
     * @return Constructed room context.
     */
    [[nodiscard]] static RoomContext from_command(
        const RoomCommand &command,
        RoomVersion roomVersion,
        EventId lastEventId,
        std::optional<NodeId> nodeId = std::nullopt,
        Timestamp now = SystemClock::now(),
        JsonObject metadata = {});

    /**
     * @brief Return the room being processed.
     *
     * @return Room identifier.
     */
    [[nodiscard]] const RoomId &room_id() const noexcept;

    /**
     * @brief Return the current room version.
     *
     * This is the version before newly produced events are persisted.
     *
     * @return Current room version.
     */
    [[nodiscard]] RoomVersion room_version() const noexcept;

    /**
     * @brief Return the last persisted event identifier.
     *
     * @return Last event identifier.
     */
    [[nodiscard]] EventId last_event_id() const noexcept;

    /**
     * @brief Return the next room version.
     *
     * @return Version expected for the first newly produced event.
     *
     * @throws vix::realtime::Error
     *         When the room version cannot be incremented.
     */
    [[nodiscard]] RoomVersion next_room_version() const;

    /**
     * @brief Return the next event identifier.
     *
     * @return Identifier expected for the next persisted event.
     *
     * @throws vix::realtime::Error
     *         When the event identifier cannot be incremented.
     */
    [[nodiscard]] EventId next_event_id() const;

    /**
     * @brief Return the logical session submitting the command.
     *
     * Lifecycle and internal operations may not have a source session.
     *
     * @return Source session identifier, when present.
     */
    [[nodiscard]] const std::optional<SessionId> &
    session_id() const noexcept;

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
     * @brief Return the runtime node owning the room.
     *
     * The value may be absent in a local runtime that does not use explicit
     * node ownership.
     *
     * @return Owning node identifier, when present.
     */
    [[nodiscard]] const std::optional<NodeId> &
    node_id() const noexcept;

    /**
     * @brief Return the timestamp assigned to this execution.
     *
     * Handlers should use this value instead of reading the system clock
     * directly when producing deterministic timestamped events.
     *
     * @return Runtime-assigned timestamp.
     */
    [[nodiscard]] Timestamp now() const noexcept;

    /**
     * @brief Return runtime-defined contextual metadata.
     *
     * Metadata may contain tracing, authorization, or adapter information.
     * It must not contain authoritative state mutations.
     *
     * @return Constant reference to contextual metadata.
     */
    [[nodiscard]] const JsonObject &
    metadata() const noexcept;

    /**
     * @brief Return whether the context contains consistent required fields.
     *
     * @return True when the context is valid.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /**
     * @brief Validate the room execution context.
     *
     * @throws vix::realtime::Error
     *         When identifiers or stream positions are inconsistent.
     */
    void validate() const;

  private:
    /** @brief Room being processed. */
    RoomId roomId_{};

    /** @brief Current room version before command execution. */
    RoomVersion roomVersion_{};

    /** @brief Last persisted event in the room stream. */
    EventId lastEventId_{};

    /** @brief Logical session submitting the operation. */
    std::optional<SessionId> sessionId_{};

    /** @brief Related client request identifier. */
    RequestId requestId_{};

    /** @brief Correlation identifier for related operations. */
    CorrelationId correlationId_{};

    /** @brief Runtime node currently owning the room. */
    std::optional<NodeId> nodeId_{};

    /** @brief Timestamp assigned by the room runtime. */
    Timestamp now_{SystemClock::now()};

    /** @brief Non-authoritative runtime metadata. */
    JsonObject metadata_{};
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_ROOM_CONTEXT_HPP
