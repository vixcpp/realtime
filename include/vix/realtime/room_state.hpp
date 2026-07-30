/**
 *
 * @file room_state.hpp
 * @author Gaspard Kirira
 * @brief Authoritative state interface for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_STATE_HPP
#define VIX_REALTIME_ROOM_STATE_HPP

#include <memory>

#include <vix/realtime/api.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Application-defined authoritative state owned by a room.
   *
   * A room state is mutated only by applying authoritative room events.
   * Client commands must never modify the state directly.
   *
   * The runtime follows this order:
   *
   * - validate and handle a command;
   * - produce authoritative events;
   * - persist the events;
   * - apply the persisted events to the state;
   * - broadcast the events to their audiences.
   *
   * Applications implement this interface for each room type.
   */
  class VIX_REALTIME_API RoomState
  {
  public:
    /**
     * @brief Destroy the room state.
     */
    virtual ~RoomState() = default;

    /**
     * @brief Return the application schema version of this state.
     *
     * The schema version is persisted in snapshots and events so applications
     * can detect incompatible or outdated stored data.
     *
     * @return Non-zero application schema version.
     */
    [[nodiscard]] virtual SchemaVersion
    schema_version() const noexcept = 0;

    /**
     * @brief Apply one authoritative event to the state.
     *
     * This method must be deterministic. Applying the same ordered event stream
     * to the same initial state must always produce the same final state.
     *
     * Implementations must not perform network calls, persistence, broadcasting,
     * or other external side effects.
     *
     * @param event Authoritative event to apply.
     *
     * @throws vix::realtime::Error
     *         When the event is unsupported, malformed, or incompatible with
     *         the current state schema.
     */
    virtual void apply(const RoomEvent &event) = 0;

    /**
     * @brief Serialize the complete authoritative state.
     *
     * The returned value is stored inside a room snapshot.
     *
     * @return Serialized room state.
     */
    [[nodiscard]] virtual JsonObject serialize() const = 0;

    /**
     * @brief Restore the complete authoritative state from serialized data.
     *
     * The implementation must replace the current state rather than merge
     * partially with existing values.
     *
     * @param state Serialized authoritative state.
     * @param schemaVersion Schema version associated with the stored state.
     *
     * @throws vix::realtime::Error
     *         When the serialized state is malformed or incompatible.
     */
    virtual void restore(
        const JsonObject &state,
        SchemaVersion schemaVersion) = 0;

    /**
     * @brief Create an independent copy of the room state.
     *
     * The copy may be used by tests, diagnostics, speculative validation, or
     * transactional runtime operations.
     *
     * @return Independently owned state copy.
     */
    [[nodiscard]] virtual std::unique_ptr<RoomState>
    clone() const = 0;
  };

  /**
   * @brief Unique ownership pointer for an application room state.
   */
  using RoomStatePtr = std::unique_ptr<RoomState>;

} // namespace vix::realtime

#endif // VIX_REALTIME_ROOM_STATE_HPP
