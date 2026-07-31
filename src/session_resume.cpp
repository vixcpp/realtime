/**
 *
 * @file session_resume.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime logical session resumption.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/session_resume.hpp>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/protocol.hpp>

namespace vix::realtime
{
  SessionResume::SessionResume(
      RoomManagerPtr manager,
      std::shared_ptr<internal::TokenGenerator> tokenGenerator)
      : manager_(std::move(manager)),
        tokenGenerator_(std::move(tokenGenerator))
  {
    if (!manager_)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "session resume service requires a room manager"};
    }

    if (!tokenGenerator_)
    {
      tokenGenerator_ =
          std::make_shared<internal::TokenGenerator>(
              internal::TokenGenerator::defaultEntropyBytes,
              "resume");
    }
  }

  SessionResume::SessionResume(
      RoomManagerPtr manager,
      std::chrono::milliseconds resumeWindow,
      std::shared_ptr<internal::TokenGenerator> tokenGenerator)
      : SessionResume(
            std::move(manager),
            std::move(tokenGenerator))
  {
    if (resumeWindow.count() < 0)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "session resume window must not be negative"};
    }

    hasCustomResumeWindow_ = true;
    customResumeWindow_ = resumeWindow;
  }

  ResumeToken SessionResume::issue(
      const SessionId &sessionId)
  {
    require_enabled();

    if (sessionId.empty())
    {
      throw Error{
          ErrorCode::SessionNotFound,
          "resume token issuance requires a session identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    SessionPtr session =
        manager_->require_session(sessionId);

    if (session->closed())
    {
      throw Error{
          ErrorCode::SessionExpired,
          "closed session cannot receive a resume token"};
    }

    ResumeToken token =
        tokenGenerator_->generate();

    session->set_resume_token(token);

    return token;
  }

  ResumeToken SessionResume::rotate(
      const SessionId &sessionId)
  {
    return issue(sessionId);
  }

  ResumeToken SessionResume::issue(
      Session &session)
  {
    require_enabled();

    if (session.id().empty() ||
        session.closed())
    {
      throw Error{
          ErrorCode::SessionExpired,
          "closed session cannot receive a resume token"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    ResumeToken token =
        tokenGenerator_->generate();

    session.set_resume_token(token);

    return token;
  }

  ResumeToken SessionResume::rotate(
      Session &session)
  {
    return issue(session);
  }

  bool SessionResume::revoke(
      const SessionId &sessionId)
  {
    if (sessionId.empty())
    {
      throw Error{
          ErrorCode::SessionNotFound,
          "resume token revocation requires a session identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    SessionPtr session =
        manager_->require_session(sessionId);

    const bool hadToken =
        !session->resume_token().empty();

    session->clear_resume_token();

    return hadToken;
  }

  bool SessionResume::revoke(
      Session &session)
  {
    if (session.id().empty())
    {
      throw Error{
          ErrorCode::SessionNotFound,
          "resume token revocation requires a session identifier"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    const bool hadToken =
        !session.resume_token().empty();

    session.clear_resume_token();

    return hadToken;
  }

  bool SessionResume::matches(
      const SessionId &sessionId,
      std::string_view token) const noexcept
  {
    try
    {
      if (!manager_->config().enableSessionResume ||
          sessionId.empty() ||
          token.empty() ||
          !tokenGenerator_->accepts(token))
      {
        return false;
      }

      std::lock_guard<std::mutex> lock{mutex_};

      SessionPtr session =
          manager_->find_session(sessionId);

      if (!session ||
          session->closed())
      {
        return false;
      }

      const ResumeToken storedToken =
          session->resume_token();

      return tokenGenerator_->accepts(storedToken) &&
             secure_equal(
                 storedToken,
                 token);
    }
    catch (...)
    {
      return false;
    }
  }

  bool SessionResume::matches(
      const Session &session,
      std::string_view token) const noexcept
  {
    try
    {
      if (!manager_->config().enableSessionResume ||
          session.id().empty() ||
          session.closed() ||
          token.empty() ||
          !tokenGenerator_->accepts(token))
      {
        return false;
      }

      std::lock_guard<std::mutex> lock{mutex_};

      const ResumeToken storedToken =
          session.resume_token();

      return tokenGenerator_->accepts(storedToken) &&
             secure_equal(
                 storedToken,
                 token);
    }
    catch (...)
    {
      return false;
    }
  }

  bool SessionResume::can_resume(
      const SessionId &sessionId,
      std::string_view token,
      Timestamp now) const noexcept
  {
    try
    {
      if (!manager_->config().enableSessionResume ||
          sessionId.empty() ||
          token.empty() ||
          !tokenGenerator_->accepts(token))
      {
        return false;
      }

      std::lock_guard<std::mutex> lock{mutex_};

      SessionPtr session =
          manager_->find_session(sessionId);

      if (!session ||
          session->closed())
      {
        return false;
      }

      const ResumeToken storedToken =
          session->resume_token();

      if (!tokenGenerator_->accepts(storedToken) ||
          !secure_equal(
              storedToken,
              token))
      {
        return false;
      }

      return session->can_resume(
          now,
          resume_window());
    }
    catch (...)
    {
      return false;
    }
  }

  bool SessionResume::can_resume(
      const Session &session,
      std::string_view token,
      Timestamp now) const noexcept
  {
    try
    {
      if (!matches(
              session,
              token))
      {
        return false;
      }

      return session.can_resume(
          now,
          resume_window());
    }
    catch (...)
    {
      return false;
    }
  }

  SessionResumeResult SessionResume::resume(
      const SessionId &sessionId,
      std::string_view token,
      ConnectionPtr connection,
      Timestamp now,
      bool rotateToken)
  {
    require_enabled();

    if (sessionId.empty())
    {
      throw Error{
          ErrorCode::InvalidResumeToken,
          "session resume requires a session identifier"};
    }

    if (!connection)
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "session resume requires a connection"};
    }

    if (connection->id().empty())
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "session resume connection requires an identifier"};
    }

    if (!connection->is_open())
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "session resume requires an open connection"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    SessionPtr session =
        manager_->find_session(sessionId);

    if (!session)
    {
      throw Error{
          ErrorCode::InvalidResumeToken,
          "session resume credential is invalid"};
    }

    validate_token_locked(
        session,
        token);

    if (session->closed())
    {
      throw Error{
          ErrorCode::SessionExpired,
          "logical session is permanently closed"};
    }

    if (session->connected())
    {
      throw Error{
          ErrorCode::InvalidResumeToken,
          "logical session already has an active connection"};
    }

    const std::optional<Timestamp> detachedAt =
        session->detached_at();

    if (!detachedAt)
    {
      throw Error{
          ErrorCode::InvalidResumeToken,
          "logical session has not been detached"};
    }

    if (now < *detachedAt)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "session resume timestamp precedes detachment"};
    }

    if (!session->can_resume(
            now,
            resume_window()))
    {
      throw Error{
          ErrorCode::SessionExpired,
          "logical session resume window has expired"};
    }

    ResumeToken nextToken =
        session->resume_token();

    if (rotateToken)
    {
      nextToken =
          tokenGenerator_->generate();
    }

    const ConnectionId connectionId =
        connection->id();

    ConnectionPtr replacedConnection =
        manager_->attach_connection(
            sessionId,
            connection,
            now);

    try
    {
      if (rotateToken)
      {
        session->set_resume_token(
            nextToken);
      }
    }
    catch (...)
    {
      try
      {
        static_cast<void>(
            manager_->detach_connection(
                sessionId,
                connectionId,
                now));
      }
      catch (...)
      {
      }

      try
      {
        connection->close(
            ErrorCode::InvalidResumeToken,
            "session resume token rotation failed");
      }
      catch (...)
      {
      }

      throw;
    }

    return SessionResumeResult{
        session,
        std::move(replacedConnection),
        std::move(nextToken),
        rotateToken};
  }

  SessionResumeResult SessionResume::resume(
      const SessionPtr &session,
      ConnectionPtr connection,
      std::string_view token,
      Timestamp now,
      bool rotateToken)
  {
    if (!session)
    {
      throw Error{
          ErrorCode::InvalidResumeToken,
          "session resume requires a session"};
    }

    return resume(
        *session,
        std::move(connection),
        token,
        now,
        rotateToken);
  }

  SessionResumeResult SessionResume::resume(
      Session &session,
      ConnectionPtr connection,
      std::string_view token,
      Timestamp now,
      bool rotateToken)
  {
    require_enabled();

    if (session.id().empty())
    {
      throw Error{
          ErrorCode::InvalidResumeToken,
          "session resume requires a session identifier"};
    }

    if (!connection)
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "session resume requires a connection"};
    }

    if (connection->id().empty())
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "session resume connection requires an identifier"};
    }

    if (!connection->is_open())
    {
      throw Error{
          ErrorCode::ConnectionNotAttached,
          "session resume requires an open connection"};
    }

    std::lock_guard<std::mutex> lock{mutex_};

    validate_token_locked(
        std::shared_ptr<Session>(
            &session,
            [](Session *) {}),
        token);

    if (session.closed())
    {
      throw Error{
          ErrorCode::SessionExpired,
          "logical session is permanently closed"};
    }

    if (session.connected())
    {
      throw Error{
          ErrorCode::InvalidResumeToken,
          "logical session already has an active connection"};
    }

    const std::optional<Timestamp> detachedAt =
        session.detached_at();

    if (!detachedAt)
    {
      throw Error{
          ErrorCode::InvalidResumeToken,
          "logical session has not been detached"};
    }

    if (now < *detachedAt)
    {
      throw Error{
          ErrorCode::CorruptedState,
          "session resume timestamp precedes detachment"};
    }

    if (!session.can_resume(
            now,
            resume_window()))
    {
      throw Error{
          ErrorCode::SessionExpired,
          "logical session resume window has expired"};
    }

    ResumeToken nextToken =
        session.resume_token();

    if (rotateToken)
    {
      nextToken =
          tokenGenerator_->generate();
    }

    ConnectionPtr replacedConnection =
        session.attach(
            connection,
            now);

    try
    {
      if (rotateToken)
      {
        session.set_resume_token(
            nextToken);
      }

      for (const RoomId &roomId : session.rooms())
      {
        EventId replayCursor =
            session.last_event_id(roomId);

        std::vector<RoomEvent> events =
            manager_->event_store()->load_after(
                roomId,
                replayCursor,
                manager_->config().maxReplayEvents + 1);

        if (events.size() >
            manager_->config().maxReplayEvents)
        {
          const auto snapshot =
              manager_->snapshot_store()->load_latest(
                  roomId);

          if (snapshot &&
              !snapshot->last_event_id().empty() &&
              (replayCursor.empty() ||
               snapshot->last_event_id() > replayCursor))
          {
            connection->send(
                protocol::from_snapshot(
                    *snapshot));

            replayCursor =
                snapshot->last_event_id();

            session.acknowledge(
                roomId,
                replayCursor);

            events =
                manager_->event_store()->load_after(
                    roomId,
                    replayCursor,
                    manager_->config().maxReplayEvents);
          }
          else
          {
            events.resize(
                manager_->config().maxReplayEvents);
          }
        }

        for (const RoomEvent &event : events)
        {
          connection->send(
              protocol::from_event(event));

          if (!event.event_id().empty())
          {
            session.acknowledge(
                roomId,
                event.event_id());
          }
        }
      }
    }
    catch (...)
    {
      try
      {
        static_cast<void>(
            session.detach(
                now));
      }
      catch (...)
      {
      }

      try
      {
        connection->close(
            ErrorCode::InvalidResumeToken,
            "session resume failed");
      }
      catch (...)
      {
      }

      throw;
    }

    return SessionResumeResult{
        std::shared_ptr<Session>(
            &session,
            [](Session *) {}),
        std::move(replacedConnection),
        std::move(nextToken),
        rotateToken};
  }

  std::chrono::milliseconds
  SessionResume::resume_window() const noexcept
  {
    if (hasCustomResumeWindow_)
    {
      return customResumeWindow_;
    }

    return std::chrono::duration_cast<
        std::chrono::milliseconds>(
        manager_->config().sessionResumeWindow);
  }

  const RoomManagerPtr &
  SessionResume::manager() const noexcept
  {
    return manager_;
  }

  const std::shared_ptr<internal::TokenGenerator> &
  SessionResume::token_generator() const noexcept
  {
    return tokenGenerator_;
  }

  void SessionResume::require_enabled() const
  {
    if (!manager_->config().enableSessionResume)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "logical session resumption is disabled"};
    }
  }

  bool SessionResume::secure_equal(
      std::string_view left,
      std::string_view right) noexcept
  {
    const std::size_t maximumSize =
        left.size() > right.size()
            ? left.size()
            : right.size();

    std::size_t difference =
        left.size() ^ right.size();

    for (std::size_t index = 0;
         index < maximumSize;
         ++index)
    {
      const unsigned char leftValue =
          index < left.size()
              ? static_cast<unsigned char>(
                    left[index])
              : 0;

      const unsigned char rightValue =
          index < right.size()
              ? static_cast<unsigned char>(
                    right[index])
              : 0;

      difference |=
          static_cast<std::size_t>(
              leftValue ^ rightValue);
    }

    return difference == 0;
  }

  void SessionResume::validate_token_locked(
      const SessionPtr &session,
      std::string_view token) const
  {
    if (!session ||
        token.empty() ||
        !tokenGenerator_->accepts(token))
    {
      throw Error{
          ErrorCode::InvalidResumeToken,
          "session resume credential is invalid"};
    }

    const ResumeToken storedToken =
        session->resume_token();

    if (storedToken.empty() ||
        !tokenGenerator_->accepts(storedToken) ||
        !secure_equal(
            storedToken,
            token))
    {
      throw Error{
          ErrorCode::InvalidResumeToken,
          "session resume credential is invalid"};
    }
  }

} // namespace vix::realtime
