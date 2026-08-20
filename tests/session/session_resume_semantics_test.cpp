#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>

#include <vix/realtime/config.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/node_id.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room_manager.hpp>
#include <vix/realtime/session_resume.hpp>

namespace vix::realtime
{
  namespace
  {
    class ResumeConnection final : public Connection
    {
    public:
      explicit ResumeConnection(std::string id)
          : id_(std::move(id))
      {
      }

      [[nodiscard]] const ConnectionId &id() const noexcept override
      {
        return id_;
      }

      [[nodiscard]] bool is_open() const noexcept override
      {
        return open_;
      }

      void send(const protocol::Envelope &) override
      {
      }

      void close(ErrorCode, std::string_view) override
      {
        open_ = false;
      }

      [[nodiscard]] JsonObject metadata() const noexcept override
      {
        return {};
      }

    private:
      ConnectionId id_{};
      bool open_{true};
    };

    [[nodiscard]] ErrorCode resume_error(auto &&operation)
    {
      try
      {
        operation();
      }
      catch (const Error &error)
      {
        return error.code();
      }

      ADD_FAILURE() << "resume unexpectedly succeeded";
      return ErrorCode::None;
    }

    struct ResumeFixture
    {
      Config config{};
      RoomManagerPtr manager{
          std::make_shared<RoomManager>(
              NodeId{std::string_view{"node-resume"}},
              config)};
      SessionResume resume{manager, std::chrono::seconds{30}};
      SessionPtr session{manager->create_session(
          SessionId{std::string_view{"session-resume"}})};

      [[nodiscard]] ResumeToken issue()
      {
        return resume.issue(*session);
      }
    };

    void expect_all_overloads(
        ResumeFixture &fixture,
        std::string_view token,
        Timestamp now,
        ErrorCode expected)
    {
      EXPECT_EQ(
          resume_error([&] {
            fixture.resume.resume(
                fixture.session->id(),
                token,
                std::make_shared<ResumeConnection>("by-id"),
                now);
          }),
          expected);

      EXPECT_EQ(
          resume_error([&] {
            fixture.resume.resume(
                fixture.session,
                std::make_shared<ResumeConnection>("by-pointer"),
                token,
                now);
          }),
          expected);

      EXPECT_EQ(
          resume_error([&] {
            fixture.resume.resume(
                *fixture.session,
                std::make_shared<ResumeConnection>("by-reference"),
                token,
                now);
          }),
          expected);
    }

    TEST(SessionResumeSemanticsTest, UnknownSessionsUseSessionNotFound)
    {
      ResumeFixture fixture;
      const Timestamp now = SystemClock::now();

      EXPECT_EQ(
          resume_error([&] {
            fixture.resume.resume(
                SessionId{std::string_view{"unknown-session"}},
                "resume-invalid-token",
                std::make_shared<ResumeConnection>("unknown-id"),
                now);
          }),
          ErrorCode::SessionNotFound);

      EXPECT_EQ(
          resume_error([&] {
            fixture.resume.resume(
                SessionPtr{},
                std::make_shared<ResumeConnection>("unknown-pointer"),
                "resume-invalid-token",
                now);
          }),
          ErrorCode::SessionNotFound);

      auto unmanaged = std::make_shared<Session>(
          SessionId{std::string_view{"unmanaged-session"}},
          Identity{"unmanaged-user"});
      const ResumeToken token = fixture.resume.issue(*unmanaged);
      unmanaged->detach(now);

      EXPECT_EQ(
          resume_error([&] {
            fixture.resume.resume(
                unmanaged,
                std::make_shared<ResumeConnection>("unmanaged-pointer"),
                token,
                now + std::chrono::seconds{1});
          }),
          ErrorCode::SessionNotFound);

      EXPECT_EQ(
          resume_error([&] {
            fixture.resume.resume(
                *unmanaged,
                std::make_shared<ResumeConnection>("unmanaged-reference"),
                token,
                now + std::chrono::seconds{1});
          }),
          ErrorCode::SessionNotFound);
    }

    TEST(SessionResumeSemanticsTest, ConnectedSessionsUseSessionAlreadyConnected)
    {
      ResumeFixture fixture;
      const ResumeToken token = fixture.issue();
      fixture.session->attach(
          std::make_shared<ResumeConnection>("active"));

      expect_all_overloads(
          fixture,
          token,
          SystemClock::now(),
          ErrorCode::SessionAlreadyConnected);
    }

    TEST(SessionResumeSemanticsTest, NeverDetachedSessionsUseSessionNotDetached)
    {
      ResumeFixture fixture;

      expect_all_overloads(
          fixture,
          fixture.issue(),
          SystemClock::now(),
          ErrorCode::SessionNotDetached);
    }

    TEST(SessionResumeSemanticsTest, InvalidTokensUseInvalidResumeToken)
    {
      ResumeFixture fixture;
      const Timestamp detachedAt = SystemClock::now();
      fixture.issue();
      fixture.session->detach(detachedAt);

      expect_all_overloads(
          fixture,
          "resume-invalid-token",
          detachedAt + std::chrono::seconds{1},
          ErrorCode::InvalidResumeToken);
    }

    TEST(SessionResumeSemanticsTest, ClosedSessionsUseSessionExpired)
    {
      ResumeFixture fixture;
      const ResumeToken token = fixture.issue();
      fixture.session->close(SystemClock::now());

      expect_all_overloads(
          fixture,
          token,
          SystemClock::now(),
          ErrorCode::SessionExpired);
    }

    TEST(SessionResumeSemanticsTest, InvalidDetachTimestampsUseCorruptedState)
    {
      ResumeFixture fixture;
      const Timestamp detachedAt = SystemClock::now();
      const ResumeToken token = fixture.issue();
      fixture.session->detach(detachedAt);

      expect_all_overloads(
          fixture,
          token,
          detachedAt - std::chrono::milliseconds{1},
          ErrorCode::CorruptedState);
    }

    TEST(SessionResumeSemanticsTest, ExpiredWindowsUseSessionExpired)
    {
      ResumeFixture fixture;
      const Timestamp detachedAt = SystemClock::now();
      const ResumeToken token = fixture.issue();
      fixture.session->detach(detachedAt);

      expect_all_overloads(
          fixture,
          token,
          detachedAt + std::chrono::seconds{31},
          ErrorCode::SessionExpired);
    }

    TEST(SessionResumeSemanticsTest, InvalidConnectionsUseConnectionNotAttached)
    {
      ResumeFixture fixture;
      const Timestamp detachedAt = SystemClock::now();
      const ResumeToken token = fixture.issue();
      fixture.session->detach(detachedAt);
      const Timestamp now = detachedAt + std::chrono::seconds{1};

      EXPECT_EQ(
          resume_error([&] {
            fixture.resume.resume(
                fixture.session->id(), token, ConnectionPtr{}, now);
          }),
          ErrorCode::ConnectionNotAttached);
      EXPECT_EQ(
          resume_error([&] {
            fixture.resume.resume(
                fixture.session, ConnectionPtr{}, token, now);
          }),
          ErrorCode::ConnectionNotAttached);
      EXPECT_EQ(
          resume_error([&] {
            fixture.resume.resume(
                *fixture.session, ConnectionPtr{}, token, now);
          }),
          ErrorCode::ConnectionNotAttached);
    }
  } // namespace
} // namespace vix::realtime
