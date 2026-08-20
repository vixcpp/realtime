#include <gtest/gtest.h>

#include <filesystem>

#ifndef VIX_REALTIME_PUBLIC_HEADERS_STAGE
#error "VIX_REALTIME_PUBLIC_HEADERS_STAGE must be defined by CMake"
#endif

TEST(PublicHeaderContract, StagedHeadersExcludeInternalDirectory)
{
  EXPECT_FALSE(std::filesystem::exists(
      std::filesystem::path{VIX_REALTIME_PUBLIC_HEADERS_STAGE} /
      "vix/realtime/internal"));
}
