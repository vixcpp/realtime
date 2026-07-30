/**
 *
 * @file event_dispatcher.hpp
 * @author Gaspard Kirira
 * @brief Transport-independent event delivery for Vix Realtime rooms.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_INTERNAL_EVENT_DISPATCHER_HPP
#define VIX_REALTIME_INTERNAL_EVENT_DISPATCHER_HPP

#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <vector>

#include <vix/realtime/room_event.hpp>
#include <vix/realtime/session_id.hpp>

namespace vix::realtime::internal
{
  /**
   * @brief Result of delivering one event to its selected sessions.
   */
  struct EventDispatchResult
  {
    /** @brief Sessions selected by the event audience. */
    std::vector<SessionId> recipients{};

    /** @brief Sessions that received the event successfully. */
    std::vector<SessionId> delivered{};

    /** @brief Sessions for which delivery failed. */
    std::vector<SessionId> failed{};

    /**
     * @brief Return the number of selected recipients.
     *
     * @return Selected recipient count.
     */
    [[nodiscard]] std::size_t recipient_count() const noexcept
    {
      return recipients.size();
    }

    /**
     * @brief Return the number of successful deliveries.
     *
     * @return Successful delivery count.
     */
    [[nodiscard]] std::size_t delivered_count() const noexcept
    {
      return delivered.size();
    }

    /**
     * @brief Return the number of failed deliveries.
     *
     * @return Failed delivery count.
     */
    [[nodiscard]] std::size_t failed_count() const noexcept
    {
      return failed.size();
    }

    /**
     * @brief Return whether every selected delivery succeeded.
     *
     * Events without recipients are considered completely dispatched.
     *
     * @return True when no delivery failed.
     */
    [[nodiscard]] bool complete() const noexcept
    {
      return failed.empty();
    }
  };

  /**
   * @brief Selects event recipients and forwards events to a delivery callback.
   *
   * The dispatcher is transport-independent. It does not know about WebSocket
   * connections, network frames, or logical session implementations.
   *
   * The room supplies its current logical session identifiers. The dispatcher
   * applies `EventAudience` rules and invokes the configured delivery handler
   * once for each selected session.
   */
  class EventDispatcher
  {
  public:
    /**
     * @brief Function used to deliver an event to one logical session.
     */
    using DeliveryHandler =
        std::function<void(const SessionId &, const RoomEvent &)>;

    /**
     * @brief Function called when one delivery handler invocation fails.
     */
    using ErrorHandler =
        std::function<void(
            const SessionId &,
            const RoomEvent &,
            std::exception_ptr)>;

    /**
     * @brief Construct a dispatcher without a delivery handler.
     */
    EventDispatcher() = default;

    /**
     * @brief Construct a dispatcher with a delivery handler.
     *
     * @param deliveryHandler Function used to deliver room events.
     */
    explicit EventDispatcher(
        DeliveryHandler deliveryHandler);

    /**
     * @brief Construct a dispatcher with delivery and error handlers.
     *
     * @param deliveryHandler Function used to deliver room events.
     * @param errorHandler Function notified when delivery fails.
     */
    EventDispatcher(
        DeliveryHandler deliveryHandler,
        ErrorHandler errorHandler);

    /**
     * @brief Replace the event delivery handler.
     *
     * @param handler New delivery handler.
     */
    void set_delivery_handler(
        DeliveryHandler handler);

    /**
     * @brief Replace the delivery error handler.
     *
     * @param handler New error handler.
     */
    void set_error_handler(
        ErrorHandler handler);

    /**
     * @brief Remove the configured delivery handler.
     */
    void clear_delivery_handler();

    /**
     * @brief Remove the configured error handler.
     */
    void clear_error_handler();

    /**
     * @brief Return whether a delivery handler is configured.
     *
     * @return True when events can be delivered.
     */
    [[nodiscard]] bool has_delivery_handler() const;

    /**
     * @brief Return whether an error handler is configured.
     *
     * @return True when delivery failures can be reported.
     */
    [[nodiscard]] bool has_error_handler() const;

    /**
     * @brief Select event recipients from current room sessions.
     *
     * Duplicate and empty session identifiers are removed.
     *
     * Audience behavior:
     *
     * - `Room`: every room session;
     * - `Sender`: only the source session;
     * - `Others`: every room session except the source session;
     * - `Session`: only the explicitly targeted session;
     * - `Internal`: no session.
     *
     * Sender and targeted sessions are selected only when they currently
     * belong to the supplied room session collection.
     *
     * @param event Event whose audience should be resolved.
     * @param roomSessions Current logical sessions in the room.
     * @return Unique selected recipients.
     *
     * @throws vix::realtime::Error
     *         When the event delivery information is inconsistent.
     */
    [[nodiscard]] static std::vector<SessionId>
    select_recipients(
        const RoomEvent &event,
        const std::vector<SessionId> &roomSessions);

    /**
     * @brief Deliver an event to every selected recipient.
     *
     * Delivery failures are isolated per recipient. A failure does not stop
     * delivery to the remaining sessions.
     *
     * @param event Persisted authoritative room event.
     * @param roomSessions Current logical sessions in the room.
     * @return Detailed delivery result.
     *
     * @throws vix::realtime::Error
     *         When event delivery information is invalid or no delivery
     *         handler is configured for a non-empty recipient collection.
     */
    [[nodiscard]] EventDispatchResult dispatch(
        const RoomEvent &event,
        const std::vector<SessionId> &roomSessions) const;

  private:
    /** @brief Protects delivery and error callbacks. */
    mutable std::mutex mutex_{};

    /** @brief Callback used to deliver events to logical sessions. */
    DeliveryHandler deliveryHandler_{};

    /** @brief Callback notified after an individual delivery failure. */
    ErrorHandler errorHandler_{};
  };

} // namespace vix::realtime::internal

#endif // VIX_REALTIME_INTERNAL_EVENT_DISPATCHER_HPP
