/**
 *
 * @file room_factory.hpp
 * @author Gaspard Kirira
 * @brief Factory interface for creating Vix Realtime room components.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_ROOM_FACTORY_HPP
#define VIX_REALTIME_ROOM_FACTORY_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <vix/realtime/api.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/room_handler.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_state.hpp>

namespace vix::realtime
{
  /**
   * @brief Components required to instantiate one Realtime room.
   *
   * Every room owns one authoritative state instance and one application
   * handler instance.
   */
  struct VIX_REALTIME_API RoomComponents
  {
    /** @brief Authoritative state owned by the room. */
    RoomStatePtr state{};

    /** @brief Application behavior owned by the room. */
    RoomHandlerPtr handler{};

    /**
     * @brief Return whether all required components are present.
     *
     * @return True when both state and handler are available.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return state != nullptr &&
             handler != nullptr;
    }

    /**
     * @brief Validate the room components.
     *
     * @throws vix::realtime::Error
     *         When the state or handler is missing.
     */
    void validate() const
    {
      if (!state)
      {
        throw Error{
            ErrorCode::MissingDependency,
            "room factory did not create a room state"};
      }

      if (!handler)
      {
        throw Error{
            ErrorCode::MissingDependency,
            "room factory did not create a room handler"};
      }
    }
  };

  /**
   * @brief Factory responsible for constructing one application room type.
   *
   * A room manager may register multiple factories under distinct room type
   * identifiers. The selected factory creates isolated state and handler
   * instances for every opened room.
   */
  class VIX_REALTIME_API RoomFactory
  {
  public:
    /**
     * @brief Maximum number of characters allowed in a room type.
     */
    static constexpr std::size_t max_type_size = 128;

    /**
     * @brief Destroy the room factory.
     */
    virtual ~RoomFactory() = default;

    /**
     * @brief Return the stable application room type.
     *
     * Examples include:
     *
     * @code
     * counter
     * chat
     * city.house
     * game.lobby
     * @endcode
     *
     * The returned view must remain valid for the lifetime of the factory.
     *
     * @return Stable room type identifier.
     */
    [[nodiscard]] virtual std::string_view
    room_type() const noexcept = 0;

    /**
     * @brief Create a fresh authoritative state for a room.
     *
     * The returned state may later be restored from a snapshot and replayed
     * events before the room becomes available.
     *
     * @param roomId Room being instantiated.
     * @return Newly owned room state.
     */
    [[nodiscard]] virtual RoomStatePtr create_state(
        const RoomId &roomId) const = 0;

    /**
     * @brief Create a fresh application handler for a room.
     *
     * @param roomId Room being instantiated.
     * @return Newly owned room handler.
     */
    [[nodiscard]] virtual RoomHandlerPtr create_handler(
        const RoomId &roomId) const = 0;

    /**
     * @brief Create and validate all components required by a room.
     *
     * @param roomId Room being instantiated.
     * @return Validated room components.
     *
     * @throws vix::realtime::Error
     *         When the room identifier, factory type, state, or handler is
     *         invalid.
     */
    [[nodiscard]] RoomComponents create(
        const RoomId &roomId) const
    {
      if (roomId.empty())
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "room factory requires a room identifier"};
      }

      if (!is_valid_type(room_type()))
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "room factory type is invalid"};
      }

      RoomComponents components{
          create_state(roomId),
          create_handler(roomId)};

      components.validate();
      return components;
    }

    /**
     * @brief Return whether a room type identifier is valid.
     *
     * Valid room types contain letters, digits, `_`, `-`, and dot-separated
     * namespaces.
     *
     * @param value Candidate room type.
     * @return True when the room type is valid.
     */
    [[nodiscard]] static bool is_valid_type(
        std::string_view value) noexcept
    {
      if (value.empty() ||
          value.size() > max_type_size)
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
        const bool alphaNumeric =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9');

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
  };

  /**
   * @brief Shared ownership pointer for a registered room factory.
   */
  using RoomFactoryPtr = std::shared_ptr<RoomFactory>;

} // namespace vix::realtime

#endif // VIX_REALTIME_ROOM_FACTORY_HPP
