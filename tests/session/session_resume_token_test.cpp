/**
 *
 * @file session_resume_token_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime session resume tokens.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <gtest/gtest.h>

#include <chrono>
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <vix/realtime/config.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room_manager.hpp>
#include <vix/realtime/session.hpp>
#include <vix/realtime/session_id.hpp>
#include <vix/realtime/session_resume.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    template <typename>
    inline constexpr bool dependentFalse = false;

    class TestConnection final : public Connection
    {
    public:
      explicit TestConnection(
          std::string identifier)
          : identifier_(
                std::move(identifier))
      {
      }

      [[nodiscard]] const ConnectionId &
      id() const noexcept override
      {
        return identifier_;
      }

      [[nodiscard]] bool
      is_open() const noexcept override
      {
        return open_;
      }

      void send(
          const protocol::Envelope &) override
      {
      }

      void close(
          ErrorCode,
          std::string_view) override
      {
        open_ = false;
      }

      [[nodiscard]] JsonObject
      metadata() const noexcept override
      {
        return metadata_;
      }

    private:
      ConnectionId identifier_{};
      bool open_{true};
      JsonObject metadata_{};
    };

    template <typename ResumeType>
    [[nodiscard]] std::unique_ptr<ResumeType>
    make_session_resume(
        const RoomManagerPtr &manager,
        std::chrono::milliseconds window)
    {
      if constexpr (
          std::constructible_from<
              ResumeType,
              RoomManagerPtr,
              std::chrono::milliseconds>)
      {
        return std::make_unique<ResumeType>(
            manager,
            window);
      }
      else if constexpr (
          std::constructible_from<
              ResumeType,
              RoomManagerPtr,
              std::chrono::seconds>)
      {
        return std::make_unique<ResumeType>(
            manager,
            std::chrono::duration_cast<
                std::chrono::seconds>(
                window));
      }
      else if constexpr (
          std::constructible_from<
              ResumeType,
              RoomManagerPtr>)
      {
        return std::make_unique<ResumeType>(
            manager);
      }
      else
      {
        static_assert(
            dependentFalse<ResumeType>,
            "Unsupported SessionResume constructor API");
      }
    }

    template <typename SessionType>
    [[nodiscard]] ResumeToken
    session_token(
        const SessionType &session)
    {
      if constexpr (
          requires {
            session.resume_token();
          })
      {
        return session.resume_token();
      }
      else if constexpr (
          requires {
            session.token();
          })
      {
        return session.token();
      }
      else
      {
        static_assert(
            dependentFalse<SessionType>,
            "Unsupported Session resume token API");
      }
    }

    template <typename ResumeType>
    [[nodiscard]] ResumeToken
    issue_token(
        ResumeType &resume,
        Session &session)
    {
      if constexpr (
          requires {
            {
              resume.issue(session)
            } -> std::convertible_to<
                ResumeToken>;
          })
      {
        return resume.issue(
            session);
      }
      else if constexpr (
          requires {
            resume.issue(session);
          })
      {
        resume.issue(
            session);

        return session_token(
            session);
      }
      else
      {
        static_assert(
            dependentFalse<ResumeType>,
            "Unsupported SessionResume issue API");
      }
    }

    template <typename ResumeType>
    [[nodiscard]] ResumeToken
    rotate_token(
        ResumeType &resume,
        Session &session)
    {
      if constexpr (
          requires {
            {
              resume.rotate(session)
            } -> std::convertible_to<
                ResumeToken>;
          })
      {
        return resume.rotate(
            session);
      }
      else if constexpr (
          requires {
            resume.rotate(session);
          })
      {
        resume.rotate(
            session);

        return session_token(
            session);
      }
      else
      {
        static_assert(
            dependentFalse<ResumeType>,
            "Unsupported SessionResume rotate API");
      }
    }

    template <typename ResumeType>
    void revoke_token(
        ResumeType &resume,
        Session &session)
    {
      if constexpr (
          requires {
            resume.revoke(
                session);
          })
      {
        resume.revoke(
            session);
      }
      else
      {
        static_assert(
            dependentFalse<ResumeType>,
            "Unsupported SessionResume revoke API");
      }
    }

    template <typename ResumeType>
    [[nodiscard]] bool token_matches(
        const ResumeType &resume,
        const Session &session,
        std::string_view token)
    {
      if constexpr (
          requires {
            {
              resume.matches(
                  session,
                  token)
            } -> std::convertible_to<bool>;
          })
      {
        return resume.matches(
            session,
            token);
      }
      else if constexpr (
          requires {
            {
              resume.matches(
                  session,
                  ResumeToken{
                      token})
            } -> std::convertible_to<bool>;
          })
      {
        return resume.matches(
            session,
            ResumeToken{
                token});
      }
      else
      {
        static_assert(
            dependentFalse<ResumeType>,
            "Unsupported SessionResume matches API");
      }
    }

    template <typename ResumeType>
    [[nodiscard]] bool can_resume_session(
        const ResumeType &resume,
        const Session &session,
        std::string_view token,
        Timestamp now)
    {
      if constexpr (
          requires {
            {
              resume.can_resume(
                  session,
                  token,
                  now)
            } -> std::convertible_to<bool>;
          })
      {
        return resume.can_resume(
            session,
            token,
            now);
      }
      else if constexpr (
          requires {
            {
              resume.can_resume(
                  session,
                  ResumeToken{
                      token},
                  now)
            } -> std::convertible_to<bool>;
          })
      {
        return resume.can_resume(
            session,
            ResumeToken{
                token},
            now);
      }
      else if constexpr (
          requires {
            {
              resume.can_resume(
                  session,
                  token)
            } -> std::convertible_to<bool>;
          })
      {
        return resume.can_resume(
            session,
            token);
      }
      else
      {
        static_assert(
            dependentFalse<ResumeType>,
            "Unsupported SessionResume can_resume API");
      }
    }

    template <typename SessionType>
    void detach_session(
        SessionType &session,
        Timestamp now)
    {
      if constexpr (
          requires {
            session.detach(
                now);
          })
      {
        session.detach(
            now);
      }
      else if constexpr (
          requires {
            session.detach();
          })
      {
        session.detach();
      }
      else
      {
        static_assert(
            dependentFalse<SessionType>,
            "Unsupported Session detach API");
      }
    }

    template <typename ResumeType>
    [[nodiscard]] SessionResumeResult
    resume_session(
        ResumeType &resume,
        const std::shared_ptr<Session> &session,
        const ConnectionPtr &connection,
        const ResumeToken &token,
        Timestamp now)
    {
      if constexpr (
          requires {
            {
              resume.resume(
                  session,
                  connection,
                  token,
                  now)
            } -> std::same_as<
                SessionResumeResult>;
          })
      {
        return resume.resume(
            session,
            connection,
            token,
            now);
      }
      else if constexpr (
          requires {
            {
              resume.resume(
                  *session,
                  connection,
                  token,
                  now)
            } -> std::same_as<
                SessionResumeResult>;
          })
      {
        return resume.resume(
            *session,
            connection,
            token,
            now);
      }
      else if constexpr (
          requires {
            {
              resume.resume(
                  session,
                  token,
                  connection,
                  now)
            } -> std::same_as<
                SessionResumeResult>;
          })
      {
        return resume.resume(
            session,
            token,
            connection,
            now);
      }
      else if constexpr (
          requires {
            {
              resume.resume(
                  session,
                  connection,
                  token)
            } -> std::same_as<
                SessionResumeResult>;
          })
      {
        return resume.resume(
            session,
            connection,
            token);
      }
      else
      {
        static_assert(
            dependentFalse<ResumeType>,
            "Unsupported SessionResume resume API");
      }
    }

    struct ResumeFixture
    {
      Config config{};

      RoomManagerPtr manager{
          std::make_shared<RoomManager>(
              NodeId{
                  std::string_view{
                      "node-1"}},
              config)};

      std::unique_ptr<SessionResume>
          resume{
              make_session_resume<
                  SessionResume>(
                  manager,
                  std::chrono::seconds{
                      30})};

      std::shared_ptr<Session>
          session{
              manager->create_session(
                  SessionId{
                      std::string_view{
                          "session-42"}},
                  Identity{
                      "user-42"})};
    };

    TEST(SessionResumeTokenTest, IssuesNonEmptyToken)
    {
      ResumeFixture fixture;

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      EXPECT_FALSE(
          token.empty());

      EXPECT_EQ(
          session_token(
              *fixture.session),
          token);
    }

    TEST(SessionResumeTokenTest, IssuedTokenMatchesSession)
    {
      ResumeFixture fixture;

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      EXPECT_TRUE(
          token_matches(
              *fixture.resume,
              *fixture.session,
              token));
    }

    TEST(SessionResumeTokenTest, DifferentTokenDoesNotMatch)
    {
      ResumeFixture fixture;

      issue_token(
          *fixture.resume,
          *fixture.session);

      EXPECT_FALSE(
          token_matches(
              *fixture.resume,
              *fixture.session,
              "resume-invalid-token"));
    }

    TEST(SessionResumeTokenTest, RotateReplacesPreviousToken)
    {
      ResumeFixture fixture;

      const ResumeToken first =
          issue_token(
              *fixture.resume,
              *fixture.session);

      const ResumeToken second =
          rotate_token(
              *fixture.resume,
              *fixture.session);

      EXPECT_FALSE(
          second.empty());

      EXPECT_NE(
          second,
          first);

      EXPECT_FALSE(
          token_matches(
              *fixture.resume,
              *fixture.session,
              first));

      EXPECT_TRUE(
          token_matches(
              *fixture.resume,
              *fixture.session,
              second));
    }

    TEST(SessionResumeTokenTest, RevokeInvalidatesToken)
    {
      ResumeFixture fixture;

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      revoke_token(
          *fixture.resume,
          *fixture.session);

      EXPECT_FALSE(
          token_matches(
              *fixture.resume,
              *fixture.session,
              token));

      EXPECT_TRUE(
          session_token(
              *fixture.session)
              .empty());
    }

    TEST(SessionResumeTokenTest, DetachedSessionCanResumeWithinWindow)
    {
      ResumeFixture fixture;

      const Timestamp now =
          SystemClock::now();

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      detach_session(
          *fixture.session,
          now);

      EXPECT_TRUE(
          can_resume_session(
              *fixture.resume,
              *fixture.session,
              token,
              now +
                  std::chrono::seconds{
                      10}));
    }

    TEST(SessionResumeTokenTest, ConnectedSessionCannotResume)
    {
      ResumeFixture fixture;

      const auto connection =
          std::make_shared<
              TestConnection>(
              "connection-1");

      fixture.session->attach(
          connection);

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      EXPECT_FALSE(
          can_resume_session(
              *fixture.resume,
              *fixture.session,
              token,
              SystemClock::now()));
    }

    TEST(SessionResumeTokenTest, ExpiredResumeWindowIsRejected)
    {
      ResumeFixture fixture;

      const Timestamp detachedAt =
          SystemClock::now();

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      detach_session(
          *fixture.session,
          detachedAt);

      EXPECT_FALSE(
          can_resume_session(
              *fixture.resume,
              *fixture.session,
              token,
              detachedAt +
                  std::chrono::minutes{
                      1}));
    }

    TEST(SessionResumeTokenTest, ResumeAttachesNewConnection)
    {
      ResumeFixture fixture;

      const auto previousConnection =
          std::make_shared<
              TestConnection>(
              "connection-1");

      const auto nextConnection =
          std::make_shared<
              TestConnection>(
              "connection-2");

      fixture.session->attach(
          previousConnection);

      const ResumeToken token =
          issue_token(
              *fixture.resume,
              *fixture.session);

      const Timestamp now =
          SystemClock::now();

      detach_session(
          *fixture.session,
          now);

      const SessionResumeResult result =
          resume_session(
              *fixture.resume,
              fixture.session,
              nextConnection,
              token,
              now +
                  std::chrono::seconds{
                      1});

      EXPECT_EQ(
          result.session,
          fixture.session);

      EXPECT_EQ(
          result.replacedConnection,
          previousConnection);

      EXPECT_TRUE(
          fixture.session->connected());

      EXPECT_TRUE(
          nextConnection->is_open());
    }

    TEST(SessionResumeTokenTest, SuccessfulResumeRotatesToken)
    {
      ResumeFixture fixture;

      const auto previousConnection =
          std::make_shared<
              TestConnection>(
              "connection-1");

      const auto nextConnection =
          std::make_shared<
              TestConnection>(
              "connection-2");

      fixture.session->attach(
          previousConnection);

      const ResumeToken previousToken =
          issue_token(
              *fixture.resume,
              *fixture.session);

      const Timestamp now =
          SystemClock::now();

      detach_session(
          *fixture.session,
          now);

      const SessionResumeResult result =
          resume_session(
              *fixture.resume,
              fixture.session,
              nextConnection,
              previousToken,
              now +
                  std::chrono::seconds{
                      1});

      EXPECT_TRUE(
          result.tokenRotated);

      EXPECT_FALSE(
          result.resumeToken.empty());

      EXPECT_NE(
          result.resumeToken,
          previousToken);

      EXPECT_EQ(
          session_token(
              *fixture.session),
          result.resumeToken);

      EXPECT_FALSE(
          token_matches(
              *fixture.resume,
              *fixture.session,
              previousToken));
    }

  } // namespace

} // namespace vix::realtime
