/**
 *
 * @file version.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#ifndef VIX_REALTIME_VERSION_HPP
#define VIX_REALTIME_VERSION_HPP

#include <string_view>

#include <vix/realtime/api.hpp>

/**
 * @file version.hpp
 * @brief Compile-time version information for the Vix Realtime module.
 */

#ifndef VIX_REALTIME_VERSION_MAJOR
#define VIX_REALTIME_VERSION_MAJOR 0
#endif

#ifndef VIX_REALTIME_VERSION_MINOR
#define VIX_REALTIME_VERSION_MINOR 1
#endif

#ifndef VIX_REALTIME_VERSION_PATCH
#define VIX_REALTIME_VERSION_PATCH 0
#endif

#define VIX_REALTIME_STRINGIFY_IMPL(value) #value
#define VIX_REALTIME_STRINGIFY(value) VIX_REALTIME_STRINGIFY_IMPL(value)

/**
 * @brief Complete semantic version string for the Realtime module.
 */
#define VIX_REALTIME_VERSION_STRING                  \
  VIX_REALTIME_STRINGIFY(VIX_REALTIME_VERSION_MAJOR) \
  "." VIX_REALTIME_STRINGIFY(VIX_REALTIME_VERSION_MINOR) "." VIX_REALTIME_STRINGIFY(VIX_REALTIME_VERSION_PATCH)

namespace vix::realtime
{
  /**
   * @brief Major version of the Realtime module.
   */
  inline constexpr int version_major = VIX_REALTIME_VERSION_MAJOR;

  /**
   * @brief Minor version of the Realtime module.
   */
  inline constexpr int version_minor = VIX_REALTIME_VERSION_MINOR;

  /**
   * @brief Patch version of the Realtime module.
   */
  inline constexpr int version_patch = VIX_REALTIME_VERSION_PATCH;

  /**
   * @brief Complete semantic version of the Realtime module.
   */
  inline constexpr std::string_view version = VIX_REALTIME_VERSION_STRING;

  /**
   * @brief Check compatibility with a requested module version.
   *
   * Compatibility requires the same major version. The current minor version
   * must be greater than or equal to the requested minor version.
   *
   * @param major Requested major version.
   * @param minor Requested minor version.
   * @return True when the requested version is compatible.
   */
  [[nodiscard]] constexpr bool version_compatible(
      int major,
      int minor = 0) noexcept
  {
    return version_major == major && version_minor >= minor;
  }

} // namespace vix::realtime

#endif // VIX_REALTIME_VERSION_HPP
