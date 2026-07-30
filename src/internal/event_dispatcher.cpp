/**
 *
 * @file event_dispatcher.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime event delivery.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/internal/event_dispatcher.hpp>

#include <algorithm>
#include <unordered_set>
#include <utility>

#include <vix/realtime/errors.hpp>

namespace vix::realtime::internal
{
  namespace
  {
    /**
     * @brief Return unique non-empty room session identifiers.
     */
    [[nodiscard]] std::vector<SessionId>
    normalize_sessions(
        const std::vector<SessionId> &sessions)
    {
      std::vector<SessionId> result;
      result.reserve(sessions.size());

      std::unordered_set<SessionId> seen;
      seen.reserve(sessions.size());

      for (const auto &sessionId : sessions)
      {
        if (sessionId.empty())
        {
          continue;
        }

        if (seen.insert(sessionId).second)
        {
          result.push_back(sessionId);
        }
      }

      return result;
    }

    /**
     * @brief Return whether a session belongs to a collection.
     */
    [[nodiscard]] bool contains_session(
        const std::vector<SessionId> &sessions,
        const SessionId &sessionId)
    {
      return std::find(
                 sessions.begin(),
                 sessions.end(),
                 sessionId) != sessions.end();
    }

  } // namespace

  EventDispatcher::EventDispatcher(
      DeliveryHandler deliveryHandler)
      : deliveryHandler_(
            std::move(deliveryHandler))
  {
  }

  EventDispatcher::EventDispatcher(
      DeliveryHandler deliveryHandler,
      ErrorHandler errorHandler)
      : deliveryHandler_(
            std::move(deliveryHandler)),
        errorHandler_(
            std::move(errorHandler))
  {
  }

  void EventDispatcher::set_delivery_handler(
      DeliveryHandler handler)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    deliveryHandler_ = std::move(handler);
  }

  void EventDispatcher::set_error_handler(
      ErrorHandler handler)
  {
    std::lock_guard<std::mutex> lock{mutex_};

    errorHandler_ = std::move(handler);
  }

  void EventDispatcher::clear_delivery_handler()
  {
    std::lock_guard<std::mutex> lock{mutex_};

    deliveryHandler_ = {};
  }

  void EventDispatcher::clear_error_handler()
  {
    std::lock_guard<std::mutex> lock{mutex_};

    errorHandler_ = {};
  }

  bool EventDispatcher::has_delivery_handler() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return static_cast<bool>(deliveryHandler_);
  }

  bool EventDispatcher::has_error_handler() const
  {
    std::lock_guard<std::mutex> lock{mutex_};

    return static_cast<bool>(errorHandler_);
  }

  std::vector<SessionId>
  EventDispatcher::select_recipients(
      const RoomEvent &event,
      const std::vector<SessionId> &roomSessions)
  {
    event.validate();

    const std::vector<SessionId> sessions =
        normalize_sessions(roomSessions);

    switch (event.audience())
    {
    case EventAudience::Room:
      return sessions;

    case EventAudience::Sender:
    {
      if (!event.source_session())
      {
        throw Error{
            ErrorCode::InternalError,
            "sender-scoped room event requires a source session"};
      }

      if (!contains_session(
              sessions,
              *event.source_session()))
      {
        return {};
      }

      return {*event.source_session()};
    }

    case EventAudience::Others:
    {
      if (!event.source_session())
      {
        throw Error{
            ErrorCode::InternalError,
            "others-scoped room event requires a source session"};
      }

      std::vector<SessionId> recipients;
      recipients.reserve(sessions.size());

      for (const auto &sessionId : sessions)
      {
        if (sessionId != *event.source_session())
        {
          recipients.push_back(sessionId);
        }
      }

      return recipients;
    }

    case EventAudience::Session:
    {
      if (!event.target_session())
      {
        throw Error{
            ErrorCode::InternalError,
            "session-scoped room event requires a target session"};
      }

      if (!contains_session(
              sessions,
              *event.target_session()))
      {
        return {};
      }

      return {*event.target_session()};
    }

    case EventAudience::Internal:
      return {};
    }

    throw Error{
        ErrorCode::InternalError,
        "room event contains an unknown audience"};
  }

  EventDispatchResult EventDispatcher::dispatch(
      const RoomEvent &event,
      const std::vector<SessionId> &roomSessions) const
  {
    EventDispatchResult result;

    result.recipients =
        select_recipients(
            event,
            roomSessions);

    if (result.recipients.empty())
    {
      return result;
    }

    DeliveryHandler deliveryHandler;
    ErrorHandler errorHandler;

    {
      std::lock_guard<std::mutex> lock{mutex_};

      deliveryHandler = deliveryHandler_;
      errorHandler = errorHandler_;
    }

    if (!deliveryHandler)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "event dispatcher requires a delivery handler"};
    }

    result.delivered.reserve(
        result.recipients.size());

    result.failed.reserve(
        result.recipients.size());

    for (const auto &sessionId : result.recipients)
    {
      try
      {
        deliveryHandler(sessionId, event);
        result.delivered.push_back(sessionId);
      }
      catch (...)
      {
        const std::exception_ptr failure =
            std::current_exception();

        result.failed.push_back(sessionId);

        if (errorHandler)
        {
          try
          {
            errorHandler(
                sessionId,
                event,
                failure);
          }
          catch (...)
          {
            // Error reporting must not interrupt remaining deliveries.
          }
        }
      }
    }

    return result;
  }

} // namespace vix::realtime::internal
