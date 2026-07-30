/**
 *
 * @file api.hpp
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

#ifndef VIX_REALTIME_API_HPP
#define VIX_REALTIME_API_HPP

/**
 * @file api.hpp
 * @brief Symbol visibility definitions for the Vix Realtime module.
 *
 * Defines the public export macro used by compiled Realtime symbols.
 *
 * Static builds do not require explicit symbol visibility and may define:
 *
 * @code
 * VIX_REALTIME_STATIC
 * @endcode
 *
 * Shared-library builds should define:
 *
 * @code
 * VIX_REALTIME_BUILD
 * @endcode
 *
 * while compiling the Realtime library itself.
 */

#if defined(VIX_REALTIME_STATIC)

#define VIX_REALTIME_API
#define VIX_REALTIME_LOCAL

#elif defined(_WIN32) || defined(__CYGWIN__)

#if defined(VIX_REALTIME_BUILD)
#define VIX_REALTIME_API __declspec(dllexport)
#else
#define VIX_REALTIME_API __declspec(dllimport)
#endif

#define VIX_REALTIME_LOCAL

#elif defined(__GNUC__) || defined(__clang__)

#define VIX_REALTIME_API __attribute__((visibility("default")))
#define VIX_REALTIME_LOCAL __attribute__((visibility("hidden")))

#else

#define VIX_REALTIME_API
#define VIX_REALTIME_LOCAL

#endif

#endif // VIX_REALTIME_API_HPP
