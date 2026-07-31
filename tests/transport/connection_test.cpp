/**
 *
 * @file connection_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime transport connection interface.
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

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <vix/json/json.hpp>
#include <vix/realtime/connection.hpp>
#include <vix/realtime/errors.hpp>
#include <vix/realtime/event_audience.hpp>
#include <vix/realtime/event_id.hpp>
#include <vix/realtime/protocol.hpp>
#include <vix/realtime/room_event.hpp>
#include <vix/realtime/room_id.hpp>
#include <vix/realtime/room_version.hpp>
#include <vix/realtime/types.hpp>

namespace vix::realtime
{
  namespace
  {
    class RecordingConnection final : public Connection
    {
    public:
      explicit RecordingConnection(
          std::string identifier,
          JsonObject metadata = {})
          : identifier_(
                std::move(identifier)),
            metadata_(
                std::move(metadata))
      {
      }

      ~RecordingConnection() override
      {
        if (destroyed_ != nullptr)
        {
          *destroyed_ = true;
        }
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
          const protocol::Envelope &envelope) override
      {
        sentEnvelopes_.push_back(
            envelope);
      }

      void close(
          ErrorCode code,
          std::string_view reason) override
      {
        open_ = false;
        closeCode_ = code;
        closeReason_ = reason;
        ++closeCount_;
      }

      [[nodiscard]] JsonObject
      metadata() const noexcept override
      {
        return metadata_;
      }

      void observe_destruction(
          bool &destroyed) noexcept
      {
        destroyed_ =
            &destroyed;
      }

      [[nodiscard]] std::size_t
      send_count() const noexcept
      {
        return sentEnvelopes_.size();
      }

      [[nodiscard]] const protocol::Envelope &
      sent_envelope(
          std::size_t index) const
      {
        return sentEnvelopes_.at(
            index);
      }

      [[nodiscard]] ErrorCode
      close_code() const noexcept
      {
        return closeCode_;
      }

      [[nodiscard]] const std::string &
      close_reason() const noexcept
      {
        return closeReason_;
      }

      [[nodiscard]] std::size_t
      close_count() const noexcept
      {
        return closeCount_;
      }

    private:
      ConnectionId identifier_{};
      bool open_{true};

      ErrorCode closeCode_{
          ErrorCode::None};

      std::string closeReason_{};
      std::size_t closeCount_{0};

      JsonObject metadata_{};

      std::vector<protocol::Envelope>
          sentEnvelopes_{};

      bool *destroyed_{nullptr};
    };

    [[nodiscard]] RoomEvent make_event()
    {
      JsonObject payload;

      payload.set_i64(
          "value",
          42);

      RoomEvent event{
          RoomId{
              std::string_view{
                  "counter/main"}},
          "counter.updated",
          std::move(payload),
          EventAudience::Room};

      event
          .set_event_id(
              EventId{
                  EventIdValue{7}})
          .set_room_version(
              RoomVersion{
                  VersionValue{7}})
          .set_request_id(
              RequestId{
                  "request-42"})
          .set_correlation_id(
              CorrelationId{
                  "correlation-84"});

      return event;
    }

    TEST(ConnectionTest, IsAbstractTransportInterface)
    {
      static_assert(
          std::is_abstract_v<
              Connection>);

      static_assert(
          std::has_virtual_destructor_v<
              Connection>);

      SUCCEED();
    }

    TEST(ConnectionTest, ExposesIdentifier)
    {
      const RecordingConnection connection{
          "connection-42"};

      EXPECT_EQ(
          connection.id(),
          "connection-42");
    }

    TEST(ConnectionTest, StartsOpen)
    {
      const RecordingConnection connection{
          "connection-42"};

      EXPECT_TRUE(
          connection.is_open());
    }

    TEST(ConnectionTest, SendsProtocolEnvelope)
    {
      RecordingConnection connection{
          "connection-42"};

      const protocol::Envelope envelope =
          protocol::from_event(
              make_event());

      connection.send(
          envelope);

      ASSERT_EQ(
          connection.send_count(),
          1U);

      const protocol::Envelope &sent =
          connection.sent_envelope(
              0);

      EXPECT_EQ(
          sent.kind(),
          protocol::MessageKind::Event);

      EXPECT_EQ(
          sent.type(),
          "counter.updated");

      ASSERT_TRUE(
          sent.room_id()
              .has_value());

      EXPECT_EQ(
          *sent.room_id(),
          RoomId{
              std::string_view{
                  "counter/main"}});

      ASSERT_TRUE(
          sent.room_version()
              .has_value());

      EXPECT_EQ(
          sent.room_version()
              ->value(),
          VersionValue{7});

      ASSERT_TRUE(
          sent.event_id()
              .has_value());

      EXPECT_EQ(
          sent.event_id()
              ->value(),
          EventIdValue{7});

      EXPECT_EQ(
          sent.request_id(),
          "request-42");

      EXPECT_EQ(
          sent.correlation_id(),
          "correlation-84");

      const auto payload =
          vix::json::to_json(
              sent.payload());

      EXPECT_EQ(
          payload.at("value")
              .get<std::int64_t>(),
          std::int64_t{42});
    }

    TEST(ConnectionTest, PreservesEnvelopeOrder)
    {
      RecordingConnection connection{
          "connection-42"};

      RoomEvent first =
          make_event();

      RoomEvent second =
          make_event();

      first.set_event_id(
          EventId{
              EventIdValue{1}});

      first.set_room_version(
          RoomVersion{
              VersionValue{1}});

      second.set_event_id(
          EventId{
              EventIdValue{2}});

      second.set_room_version(
          RoomVersion{
              VersionValue{2}});

      connection.send(
          protocol::from_event(
              first));

      connection.send(
          protocol::from_event(
              second));

      ASSERT_EQ(
          connection.send_count(),
          2U);

      ASSERT_TRUE(
          connection.sent_envelope(
                        0)
              .event_id()
              .has_value());

      ASSERT_TRUE(
          connection.sent_envelope(
                        1)
              .event_id()
              .has_value());

      EXPECT_EQ(
          connection.sent_envelope(
                        0)
              .event_id()
              ->value(),
          EventIdValue{1});

      EXPECT_EQ(
          connection.sent_envelope(
                        1)
              .event_id()
              ->value(),
          EventIdValue{2});
    }

    TEST(ConnectionTest, ClosesWithExplicitError)
    {
      RecordingConnection connection{
          "connection-42"};

      connection.close(
          ErrorCode::TransportFailure,
          "socket unavailable");

      EXPECT_FALSE(
          connection.is_open());

      EXPECT_EQ(
          connection.close_code(),
          ErrorCode::TransportFailure);

      EXPECT_EQ(
          connection.close_reason(),
          "socket unavailable");

      EXPECT_EQ(
          connection.close_count(),
          1U);
    }

    TEST(ConnectionTest, UsesDefaultCloseValuesThroughInterface)
    {
      RecordingConnection concrete{
          "connection-42"};

      Connection &connection =
          concrete;

      connection.close();

      EXPECT_FALSE(
          concrete.is_open());

      EXPECT_EQ(
          concrete.close_code(),
          ErrorCode::Cancelled);

      EXPECT_TRUE(
          concrete.close_reason()
              .empty());

      EXPECT_EQ(
          concrete.close_count(),
          1U);
    }

    TEST(ConnectionTest, RepeatedCloseRemainsClosed)
    {
      RecordingConnection connection{
          "connection-42"};

      connection.close(
          ErrorCode::Cancelled,
          "first close");

      connection.close(
          ErrorCode::TransportFailure,
          "second close");

      EXPECT_FALSE(
          connection.is_open());

      EXPECT_EQ(
          connection.close_count(),
          2U);

      EXPECT_EQ(
          connection.close_code(),
          ErrorCode::TransportFailure);

      EXPECT_EQ(
          connection.close_reason(),
          "second close");
    }

    TEST(ConnectionTest, ExposesMetadata)
    {
      JsonObject metadata;

      metadata.set_string(
          "remoteAddress",
          "127.0.0.1");

      metadata.set_string(
          "transport",
          "websocket");

      const RecordingConnection connection{
          "connection-42",
          std::move(metadata)};

      const auto json =
          vix::json::to_json(
              connection.metadata());

      EXPECT_EQ(
          json.at("remoteAddress")
              .get<std::string>(),
          "127.0.0.1");

      EXPECT_EQ(
          json.at("transport")
              .get<std::string>(),
          "websocket");
    }

    TEST(ConnectionTest, SupportsSharedConnectionAlias)
    {
      ConnectionPtr connection =
          std::make_shared<
              RecordingConnection>(
              "connection-42");

      ASSERT_NE(
          connection,
          nullptr);

      EXPECT_EQ(
          connection->id(),
          "connection-42");

      EXPECT_TRUE(
          connection->is_open());
    }

    TEST(ConnectionTest, DestroysImplementationThroughBasePointer)
    {
      bool destroyed = false;

      auto concrete =
          std::make_unique<
              RecordingConnection>(
              "connection-42");

      concrete->observe_destruction(
          destroyed);

      std::unique_ptr<Connection>
          connection{
              std::move(concrete)};

      connection.reset();

      EXPECT_TRUE(
          destroyed);
    }

  } // namespace

} // namespace vix::realtime
