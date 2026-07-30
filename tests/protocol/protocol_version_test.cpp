/**
 *
 * @file protocol_version_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for Vix Realtime protocol versions and message kinds.
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

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

#include <vix/realtime/protocol.hpp>

namespace vix::realtime::protocol
{
  namespace
  {
    TEST(ProtocolVersionTest, StoresMajorAndMinorValues)
    {
      const Version version{
          1,
          0};

      EXPECT_EQ(
          version.major,
          std::uint32_t{1});

      EXPECT_EQ(
          version.minor,
          std::uint32_t{0});
    }

    TEST(ProtocolVersionTest, SupportsEquality)
    {
      const Version first{
          1,
          0};

      const Version second{
          1,
          0};

      const Version different{
          2,
          0};

      EXPECT_EQ(first, second);
      EXPECT_NE(first, different);
    }

    TEST(ProtocolVersionTest, SupportsOrdering)
    {
      const Version older{
          1,
          0};

      const Version newerMinor{
          1,
          1};

      const Version newerMajor{
          2,
          0};

      EXPECT_LT(
          older,
          newerMinor);

      EXPECT_LT(
          newerMinor,
          newerMajor);

      EXPECT_GT(
          newerMajor,
          older);
    }

    TEST(ProtocolVersionTest, SupportsCurrentVersion)
    {
      EXPECT_TRUE(
          is_supported(
              Version{
                  1,
                  0}));
    }

    TEST(ProtocolVersionTest, RejectsOlderMajorVersion)
    {
      EXPECT_FALSE(
          is_supported(
              Version{
                  0,
                  0}));
    }

    TEST(ProtocolVersionTest, RejectsNewerMajorVersion)
    {
      EXPECT_FALSE(
          is_supported(
              Version{
                  2,
                  0}));
    }

    TEST(ProtocolVersionTest, RejectsUnsupportedMinorVersion)
    {
      EXPECT_FALSE(
          is_supported(
              Version{
                  1,
                  1}));
    }

    TEST(ProtocolMessageKindTest, UsesUnsignedByteStorage)
    {
      EXPECT_TRUE(
          (std::is_same_v<
              std::underlying_type_t<
                  MessageKind>,
              std::uint8_t>));
    }

    TEST(ProtocolMessageKindTest, ValuesAreStable)
    {
      EXPECT_EQ(
          static_cast<std::uint8_t>(
              MessageKind::Request),
          std::uint8_t{0});

      EXPECT_EQ(
          static_cast<std::uint8_t>(
              MessageKind::Response),
          std::uint8_t{1});

      EXPECT_EQ(
          static_cast<std::uint8_t>(
              MessageKind::Event),
          std::uint8_t{2});

      EXPECT_EQ(
          static_cast<std::uint8_t>(
              MessageKind::Error),
          std::uint8_t{3});

      EXPECT_EQ(
          static_cast<std::uint8_t>(
              MessageKind::Snapshot),
          std::uint8_t{4});

      EXPECT_EQ(
          static_cast<std::uint8_t>(
              MessageKind::Control),
          std::uint8_t{5});
    }

    TEST(ProtocolMessageKindTest, ConvertsKindsToStableStrings)
    {
      EXPECT_EQ(
          to_string(
              MessageKind::Request),
          std::string_view{
              "request"});

      EXPECT_EQ(
          to_string(
              MessageKind::Response),
          std::string_view{
              "response"});

      EXPECT_EQ(
          to_string(
              MessageKind::Event),
          std::string_view{
              "event"});

      EXPECT_EQ(
          to_string(
              MessageKind::Error),
          std::string_view{
              "error"});

      EXPECT_EQ(
          to_string(
              MessageKind::Snapshot),
          std::string_view{
              "snapshot"});

      EXPECT_EQ(
          to_string(
              MessageKind::Control),
          std::string_view{
              "control"});
    }

    TEST(ProtocolMessageKindTest, ParsesRequestKind)
    {
      const std::optional<MessageKind> kind =
          parse_message_kind(
              "request");

      ASSERT_TRUE(
          kind.has_value());

      EXPECT_EQ(
          *kind,
          MessageKind::Request);
    }

    TEST(ProtocolMessageKindTest, ParsesResponseKind)
    {
      const std::optional<MessageKind> kind =
          parse_message_kind(
              "response");

      ASSERT_TRUE(
          kind.has_value());

      EXPECT_EQ(
          *kind,
          MessageKind::Response);
    }

    TEST(ProtocolMessageKindTest, ParsesEventKind)
    {
      const std::optional<MessageKind> kind =
          parse_message_kind(
              "event");

      ASSERT_TRUE(
          kind.has_value());

      EXPECT_EQ(
          *kind,
          MessageKind::Event);
    }

    TEST(ProtocolMessageKindTest, ParsesErrorKind)
    {
      const std::optional<MessageKind> kind =
          parse_message_kind(
              "error");

      ASSERT_TRUE(
          kind.has_value());

      EXPECT_EQ(
          *kind,
          MessageKind::Error);
    }

    TEST(ProtocolMessageKindTest, ParsesSnapshotKind)
    {
      const std::optional<MessageKind> kind =
          parse_message_kind(
              "snapshot");

      ASSERT_TRUE(
          kind.has_value());

      EXPECT_EQ(
          *kind,
          MessageKind::Snapshot);
    }

    TEST(ProtocolMessageKindTest, ParsesControlKind)
    {
      const std::optional<MessageKind> kind =
          parse_message_kind(
              "control");

      ASSERT_TRUE(
          kind.has_value());

      EXPECT_EQ(
          *kind,
          MessageKind::Control);
    }

    TEST(ProtocolMessageKindTest, RejectsUnknownKind)
    {
      EXPECT_FALSE(
          parse_message_kind(
              "unknown")
              .has_value());

      EXPECT_FALSE(
          parse_message_kind("")
              .has_value());

      EXPECT_FALSE(
          parse_message_kind(
              "EVENT")
              .has_value());
    }

    TEST(ProtocolMessageKindTest, ConversionIsConstexpr)
    {
      constexpr std::string_view request =
          to_string(
              MessageKind::Request);

      constexpr std::string_view event =
          to_string(
              MessageKind::Event);

      static_assert(
          request == "request");

      static_assert(
          event == "event");

      SUCCEED();
    }

  } // namespace

} // namespace vix::realtime::protocol
