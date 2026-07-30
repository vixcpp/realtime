/**
 *
 * @file node_id_test.cpp
 * @author Gaspard Kirira
 * @brief Tests for the Vix Realtime runtime node identifier.
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

#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>

#include <vix/realtime/errors.hpp>
#include <vix/realtime/node_id.hpp>

namespace vix::realtime
{
  namespace
  {
    TEST(NodeIdTest, DefaultIdentifierIsEmpty)
    {
      const NodeId nodeId;

      EXPECT_TRUE(nodeId.empty());
      EXPECT_TRUE(nodeId.value().empty());
      EXPECT_TRUE(nodeId.view().empty());
      EXPECT_EQ(nodeId.size(), 0U);
    }

    TEST(NodeIdTest, AcceptsSimpleIdentifier)
    {
      const NodeId nodeId{
          std::string_view{"node1"}};

      EXPECT_FALSE(nodeId.empty());

      EXPECT_EQ(
          nodeId.value(),
          "node1");

      EXPECT_EQ(
          nodeId.view(),
          "node1");

      EXPECT_EQ(
          nodeId.size(),
          5U);
    }

    TEST(NodeIdTest, AcceptsSupportedSeparators)
    {
      const NodeId nodeId{
          std::string_view{
              "africa_node-1.main:primary"}};

      EXPECT_EQ(
          nodeId.value(),
          "africa_node-1.main:primary");
    }

    TEST(NodeIdTest, AcceptsMaximumLength)
    {
      const std::string value(
          NodeId::max_size,
          'a');

      const NodeId nodeId{
          std::string_view{value}};

      EXPECT_EQ(
          nodeId.size(),
          NodeId::max_size);

      EXPECT_EQ(
          nodeId.value(),
          value);
    }

    TEST(NodeIdTest, RejectsExplicitEmptyIdentifier)
    {
      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{}}),
          Error);
    }

    TEST(NodeIdTest, RejectsIdentifierAboveMaximumLength)
    {
      const std::string value(
          NodeId::max_size + 1,
          'a');

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{value}}),
          Error);
    }

    TEST(NodeIdTest, RejectsWhitespace)
    {
      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node one"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node\tone"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node\none"}}),
          Error);
    }

    TEST(NodeIdTest, RejectsUnsupportedCharacters)
    {
      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node/one"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node@one"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node#one"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node?one"}}),
          Error);
    }

    TEST(NodeIdTest, RejectsLeadingSeparator)
    {
      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "-node"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "_node"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      ".node"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      ":node"}}),
          Error);
    }

    TEST(NodeIdTest, RejectsTrailingSeparator)
    {
      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node-"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node_"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node."}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node:"}}),
          Error);
    }

    TEST(NodeIdTest, RejectsConsecutiveSeparators)
    {
      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node--one"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node__one"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node..one"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node::one"}}),
          Error);

      EXPECT_THROW(
          static_cast<void>(
              NodeId{
                  std::string_view{
                      "node-:one"}}),
          Error);
    }

    TEST(NodeIdTest, StaticValidationAcceptsValidIdentifier)
    {
      EXPECT_TRUE(
          NodeId::is_valid(
              "node-1"));

      EXPECT_TRUE(
          NodeId::is_valid(
              "africa_node.main:primary"));
    }

    TEST(NodeIdTest, StaticValidationRejectsInvalidIdentifier)
    {
      EXPECT_FALSE(
          NodeId::is_valid(""));

      EXPECT_FALSE(
          NodeId::is_valid(
              "-node"));

      EXPECT_FALSE(
          NodeId::is_valid(
              "node--1"));

      EXPECT_FALSE(
          NodeId::is_valid(
              "node/1"));
    }

    TEST(NodeIdTest, SupportsEquality)
    {
      const NodeId first{
          std::string_view{
              "node-1"}};

      const NodeId second{
          std::string_view{
              "node-1"}};

      const NodeId different{
          std::string_view{
              "node-2"}};

      EXPECT_EQ(first, second);
      EXPECT_NE(first, different);
    }

    TEST(NodeIdTest, SupportsLexicographicalOrdering)
    {
      const NodeId first{
          std::string_view{
              "node-1"}};

      const NodeId second{
          std::string_view{
              "node-2"}};

      EXPECT_LT(first, second);
      EXPECT_GT(second, first);
      EXPECT_LE(first, second);
      EXPECT_GE(second, first);
    }

    TEST(NodeIdTest, HashIsStableForEqualIdentifiers)
    {
      const NodeId first{
          std::string_view{
              "node-1"}};

      const NodeId second{
          std::string_view{
              "node-1"}};

      const std::hash<NodeId> hash;

      EXPECT_EQ(
          hash(first),
          hash(second));
    }

    TEST(NodeIdTest, CanBeUsedInUnorderedContainers)
    {
      std::unordered_set<NodeId> nodeIds;

      nodeIds.emplace(
          std::string_view{
              "node-1"});

      nodeIds.emplace(
          std::string_view{
              "node-2"});

      nodeIds.emplace(
          std::string_view{
              "node-1"});

      EXPECT_EQ(
          nodeIds.size(),
          2U);

      EXPECT_EQ(
          nodeIds.count(
              NodeId{
                  std::string_view{
                      "node-1"}}),
          1U);

      EXPECT_EQ(
          nodeIds.count(
              NodeId{
                  std::string_view{
                      "node-unknown"}}),
          0U);
    }

    TEST(NodeIdTest, ConvertsIdentifierToString)
    {
      const NodeId nodeId{
          std::string_view{
              "node-1"}};

      EXPECT_EQ(
          to_string(nodeId),
          "node-1");
    }

  } // namespace

} // namespace vix::realtime
