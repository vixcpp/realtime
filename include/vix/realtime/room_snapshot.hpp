/**
 *
 * @file room_snapshot.hpp
 * @author Gaspard Kirira
 * @brief Persistent state snapshot for a Vix Realtime room.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_SNAPSHOT_HPP
#define VIX_REALTIME_ROOM_SNAPSHOT_HPP

#include <optional>
#include <string>

#include <vix/realtime/api.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  /**
   * @brief Represents one persistent snapshot of a room state.
   *
   * A snapshot captures the serialized state of a room at a specific room
   * version and event position. When restoring a room, the runtime loads the
   * latest compatible snapshot and replays events that occurred after it.
   */
  class VIX_REALTIME_API RoomSnapshot
  {
  public:
    /**
     * @brief Construct an empty room snapshot.
     *
     * The resulting snapshot is not valid until all required fields are set.
     */
    RoomSnapshot() = default;

    /**
     * @brief Construct a room snapshot.
     *
     * @param roomId Room owning the snapshot.
     * @param roomVersion Version represented by the serialized state.
     * @param lastEventId Last event included in the serialized state.
     * @param state Serialized room state.
     * @param schemaVersion Application snapshot schema version.
     */
    RoomSnapshot(
        RoomId roomId,
        RoomVersion roomVersion,
        EventId lastEventId,
        JsonObject state,
        SchemaVersion schemaVersion = 1);

    /**
     * @brief Return the room owning the snapshot.
     *
     * @return Room identifier.
     */
    [[nodiscard]] const RoomId &room_id() const noexcept;

    /**
     * @brief Return the room version represented by the snapshot.
     *
     * @return Room version.
     */
    [[nodiscard]] RoomVersion room_version() const noexcept;

    /**
     * @brief Return the last event included in the snapshot.
     *
     * @return Last persisted event identifier.
     */
    [[nodiscard]] EventId last_event_id() const noexcept;

    /**
     * @brief Return the serialized room state.
     *
     * @return Constant reference to the snapshot state.
     */
    [[nodiscard]] const JsonObject &state() const noexcept;

    /**
     * @brief Return mutable access to the serialized room state.
     *
     * This is intended for snapshot construction before persistence. A stored
     * snapshot must be treated as immutable.
     *
     * @return Mutable reference to the snapshot state.
     */
    [[nodiscard]] JsonObject &state() noexcept;

    /**
     * @brief Return the snapshot schema version.
     *
     * @return Application snapshot schema version.
     */
    [[nodiscard]] SchemaVersion schema_version() const noexcept;

    /**
     * @brief Return the snapshot creation timestamp.
     *
     * @return Snapshot creation timestamp.
     */
    [[nodiscard]] Timestamp created_at() const noexcept;

    /**
     * @brief Return the optional snapshot checksum.
     *
     * @return Checksum string, when present.
     */
    [[nodiscard]] const std::optional<std::string> &
    checksum() const noexcept;

    /**
     * @brief Return application-defined snapshot metadata.
     *
     * @return Constant reference to snapshot metadata.
     */
    [[nodiscard]] const JsonObject &metadata() const noexcept;

    /**
     * @brief Assign the room version represented by the snapshot.
     *
     * @param value Room version.
     * @return Current snapshot.
     */
    RoomSnapshot &set_room_version(RoomVersion value) noexcept;

    /**
     * @brief Assign the last event included in the snapshot.
     *
     * @param value Last event identifier.
     * @return Current snapshot.
     */
    RoomSnapshot &set_last_event_id(EventId value) noexcept;

    /**
     * @brief Set the snapshot schema version.
     *
     * @param value Non-zero schema version.
     * @return Current snapshot.
     */
    RoomSnapshot &set_schema_version(SchemaVersion value);

    /**
     * @brief Set the snapshot creation timestamp.
     *
     * @param value Snapshot creation timestamp.
     * @return Current snapshot.
     */
    RoomSnapshot &set_created_at(Timestamp value) noexcept;

    /**
     * @brief Set the snapshot checksum.
     *
     * @param value Checksum value.
     * @return Current snapshot.
     */
    RoomSnapshot &set_checksum(std::string value);

    /**
     * @brief Remove the snapshot checksum.
     *
     * @return Current snapshot.
     */
    RoomSnapshot &clear_checksum() noexcept;

    /**
     * @brief Replace application-defined snapshot metadata.
     *
     * @param value Snapshot metadata.
     * @return Current snapshot.
     */
    RoomSnapshot &set_metadata(JsonObject value);

    /**
     * @brief Return whether the snapshot contains valid required fields.
     *
     * @return True when the snapshot is valid.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /**
     * @brief Validate the snapshot.
     *
     * Validation ensures:
     *
     * - the room identifier is present;
     * - the schema version is non-zero;
     * - the room version and event identifier are consistent;
     * - an optional checksum is not empty.
     *
     * @throws vix::realtime::Error
     *         When the snapshot is inconsistent.
     */
    void validate() const;

  private:
    /** @brief Room owning the snapshot. */
    RoomId roomId_{};

    /** @brief Room version represented by the serialized state. */
    RoomVersion roomVersion_{};

    /** @brief Last persisted event included in the snapshot. */
    EventId lastEventId_{};

    /** @brief Serialized authoritative room state. */
    JsonObject state_{};

    /** @brief Application snapshot schema version. */
    SchemaVersion schemaVersion_{1};

    /** @brief Time at which the snapshot was created. */
    Timestamp createdAt_{SystemClock::now()};

    /** @brief Optional integrity checksum. */
    std::optional<std::string> checksum_{};

    /** @brief Non-authoritative snapshot metadata. */
    JsonObject metadata_{};
  };

} // namespace vix::realtime

#endif // VIX_REALTIME_ROOM_SNAPSHOT_HPP
