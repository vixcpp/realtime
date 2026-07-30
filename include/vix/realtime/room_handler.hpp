/**
 *
 * @file room_handler.hpp
 * @author Gaspard Kirira
 * @brief Application command and lifecycle handler for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_HANDLER_HPP
#define VIX_REALTIME_ROOM_HANDLER_HPP

#include <memory>

#include <vix/realtime/api.hpp>
#include <vix/realtime/command_result.hpp>
#include <vix/realtime/room_command.hpp>
#include <vix/realtime/room_context.hpp>
#include <vix/realtime/room_state.hpp>
#include <vix/realtime/session_id.hpp>

namespace vix::realtime
{
  /**
   * @brief Application-defined behavior for one Realtime room type.
   *
   * A room handler receives commands and lifecycle notifications and produces
   * authoritative room events through `CommandResult`.
   *
   * The handler must never mutate `RoomState` directly. The runtime persists
   * produced events before applying them to the authoritative state.
   *
   * Handler implementations should remain deterministic and must not perform
   * direct broadcasting, transport access, or room persistence.
   */
  class VIX_REALTIME_API RoomHandler
  {
  public:
    /**
     * @brief Destroy the room handler.
     */
    virtual ~RoomHandler() = default;

    /**
     * @brief Process one client command.
     *
     * The supplied state represents the authoritative room state before the
     * command is processed.
     *
     * @param command Client command to process.
     * @param state Current authoritative room state.
     * @param context Immutable runtime execution context.
     * @return Accepted, rejected, or ignored command result.
     */
    [[nodiscard]] virtual CommandResult handle_command(
        const RoomCommand &command,
        const RoomState &state,
        const RoomContext &context) = 0;

    /**
     * @brief Handle the opening of a room.
     *
     * This callback runs after state restoration and before the room becomes
     * available to sessions.
     *
     * The default implementation accepts the lifecycle operation without
     * producing events.
     *
     * @param state Restored authoritative room state.
     * @param context Room lifecycle context.
     * @return Lifecycle result.
     */
    [[nodiscard]] virtual CommandResult on_open(
        const RoomState &state,
        const RoomContext &context)
    {
      static_cast<void>(state);
      static_cast<void>(context);

      return CommandResult::accepted();
    }

    /**
     * @brief Handle a logical session joining the room.
     *
     * The default implementation accepts the join without producing events.
     *
     * @param sessionId Joining logical session.
     * @param state Current authoritative room state.
     * @param context Room lifecycle context.
     * @return Lifecycle result.
     */
    [[nodiscard]] virtual CommandResult on_join(
        const SessionId &sessionId,
        const RoomState &state,
        const RoomContext &context)
    {
      static_cast<void>(sessionId);
      static_cast<void>(state);
      static_cast<void>(context);

      return CommandResult::accepted();
    }

    /**
     * @brief Handle a logical session leaving the room.
     *
     * The default implementation accepts the leave without producing events.
     *
     * @param sessionId Leaving logical session.
     * @param state Current authoritative room state.
     * @param context Room lifecycle context.
     * @return Lifecycle result.
     */
    [[nodiscard]] virtual CommandResult on_leave(
        const SessionId &sessionId,
        const RoomState &state,
        const RoomContext &context)
    {
      static_cast<void>(sessionId);
      static_cast<void>(state);
      static_cast<void>(context);

      return CommandResult::accepted();
    }

    /**
     * @brief Handle the closing of a room.
     *
     * This callback runs before the final optional snapshot is persisted.
     *
     * The default implementation accepts the close without producing events.
     *
     * @param state Current authoritative room state.
     * @param context Room lifecycle context.
     * @return Lifecycle result.
     */
    [[nodiscard]] virtual CommandResult on_close(
        const RoomState &state,
        const RoomContext &context)
    {
      static_cast<void>(state);
      static_cast<void>(context);

      return CommandResult::accepted();
    }
  };

  /**
   * @brief Unique ownership pointer for an application room handler.
   */
  using RoomHandlerPtr = std::unique_ptr<RoomHandler>;

} // namespace vix::realtime

#endif // VIX_REALTIME_ROOM_HANDLER_HPP
