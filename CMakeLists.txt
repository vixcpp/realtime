# ====================================================================
#
#  Vix.cpp — Realtime module
#
#  Public targets:
#    - vix_realtime
#    - vix::realtime
#
#  The realtime core provides:
#    - stateful rooms
#    - commands and events
#    - snapshots and replay
#    - logical sessions and presence
#    - interchangeable persistence stores
#
#  WebSocket support is provided through an optional transport adapter.
#  PostgreSQL support is optional.
#
# ====================================================================

cmake_minimum_required(VERSION 3.20)

project(
  vix_realtime
  VERSION 0.1.0
  LANGUAGES CXX
)

include(GNUInstallDirs)
include(FetchContent)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --------------------------------------------------------------------
# Options
# --------------------------------------------------------------------

option(
  VIX_REALTIME_FETCH_DEPENDENCIES
  "Fetch missing Vix dependencies"
  ON
)

option(
  VIX_REALTIME_WITH_WEBSOCKET
  "Build the Vix WebSocket transport adapter"
  ON
)

option(
  VIX_REALTIME_WITH_POSTGRES
  "Build PostgreSQL event and snapshot stores"
  OFF
)

option(
  VIX_REALTIME_BUILD_TESTS
  "Build realtime module tests"
  OFF
)

option(
  VIX_REALTIME_BUILD_EXAMPLES
  "Build realtime module examples"
  OFF
)

option(
  VIX_REALTIME_ENABLE_INSTALL
  "Generate install rules for the realtime standalone package"
  OFF
)

if(NOT VIX_REALTIME_ENABLE_INSTALL)
  set(CMAKE_SKIP_INSTALL_RULES TRUE)
endif()

if(VIX_UMBRELLA_BUILD)
  set(VIX_REALTIME_EXPORT_SET VixTargets)
else()
  set(VIX_REALTIME_EXPORT_SET VixRealtimeTargets)
endif()

# --------------------------------------------------------------------
# Dependency resolution
# --------------------------------------------------------------------

function(vix_realtime_find_target output_variable)
  foreach(candidate IN LISTS ARGN)
    if(TARGET "${candidate}")
      set(
        "${output_variable}"
        "${candidate}"
        PARENT_SCOPE
      )

      return()
    endif()
  endforeach()

  set(
    "${output_variable}"
    ""
    PARENT_SCOPE
  )
endfunction()

function(vix_realtime_resolve_dependency module output_variable)
  set(candidate_targets ${ARGN})

  vix_realtime_find_target(
    resolved_target
    ${candidate_targets}
  )

  if(resolved_target)
    set(
      "${output_variable}"
      "${resolved_target}"
      PARENT_SCOPE
    )

    return()
  endif()

  set(
    sibling_directory
    "${CMAKE_CURRENT_LIST_DIR}/../${module}"
  )

  if(EXISTS "${sibling_directory}/CMakeLists.txt")
    message(
      STATUS
      "[realtime] Adding ${module} from sibling: ../${module}"
    )

    add_subdirectory(
      "${sibling_directory}"
      "${CMAKE_CURRENT_BINARY_DIR}/dependencies/${module}"
    )
  elseif(VIX_REALTIME_FETCH_DEPENDENCIES)
    string(
      MAKE_C_IDENTIFIER
      "vix_realtime_${module}_external"
      fetch_name
    )

    message(
      STATUS
      "[realtime] Fetching vix::${module} from GitHub"
    )

    FetchContent_Declare(
      ${fetch_name}
      GIT_REPOSITORY "https://github.com/vixcpp/${module}.git"
      GIT_TAG dev
      GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(
      ${fetch_name}
    )
  else()
    message(
      FATAL_ERROR
      "[realtime] Missing dependency: ${module}. "
      "Provide it before realtime or enable "
      "VIX_REALTIME_FETCH_DEPENDENCIES."
    )
  endif()

  vix_realtime_find_target(
    resolved_target
    ${candidate_targets}
  )

  if(NOT resolved_target)
    message(
      FATAL_ERROR
      "[realtime] Dependency '${module}' was loaded, but none of "
      "the expected targets were created: ${candidate_targets}"
    )
  endif()

  set(
    "${output_variable}"
    "${resolved_target}"
    PARENT_SCOPE
  )
endfunction()

# JSON is resolved first because other Vix modules may require it
# while they are being configured.

vix_realtime_resolve_dependency(
  json
  VIX_REALTIME_JSON_TARGET
  vix::json
  vix_json
)

vix_realtime_resolve_dependency(
  core
  VIX_REALTIME_CORE_TARGET
  vix::core
  vix_core
)

vix_realtime_resolve_dependency(
  error
  VIX_REALTIME_ERROR_TARGET
  vix::error
  vix_error
)

vix_realtime_resolve_dependency(
  async
  VIX_REALTIME_ASYNC_TARGET
  vix::async
  vix_async
)

vix_realtime_resolve_dependency(
  sync
  VIX_REALTIME_SYNC_TARGET
  vix::sync
  vix_sync
)

vix_realtime_resolve_dependency(
  time
  VIX_REALTIME_TIME_TARGET
  vix::time
  vix_time
)

vix_realtime_resolve_dependency(
  utils
  VIX_REALTIME_UTILS_TARGET
  vix::utils
  vix_utils
)

if(VIX_REALTIME_WITH_WEBSOCKET)
  vix_realtime_resolve_dependency(
    websocket
    VIX_REALTIME_WEBSOCKET_TARGET
    vix::websocket
    vix_websocket
  )
endif()

if(VIX_REALTIME_WITH_POSTGRES)
  vix_realtime_resolve_dependency(
    db
    VIX_REALTIME_DB_TARGET
    vix::db
    vix_db
  )
endif()

find_package(
  Threads
  REQUIRED
)

# --------------------------------------------------------------------
# Sources and headers
# --------------------------------------------------------------------

file(
  GLOB_RECURSE
  REALTIME_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(
  GLOB_RECURSE
  REALTIME_PUBLIC_HEADERS
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

if(NOT VIX_REALTIME_WITH_WEBSOCKET)
  list(
    FILTER REALTIME_SOURCES
    EXCLUDE REGEX
    "[/\\\\]websocket_adapter\\.cpp$"
  )
endif()

if(NOT VIX_REALTIME_WITH_POSTGRES)
  list(
    FILTER REALTIME_SOURCES
    EXCLUDE REGEX
    "[/\\\\]postgres_(event|snapshot)_store\\.cpp$"
  )
endif()

# --------------------------------------------------------------------
# Library target
# --------------------------------------------------------------------

if(REALTIME_SOURCES)
  message(
    STATUS
    "[realtime] Building STATIC library with detected sources"
  )

  add_library(
    vix_realtime
    STATIC
    ${REALTIME_SOURCES}
    ${REALTIME_PUBLIC_HEADERS}
  )

  set(
    VIX_REALTIME_LINK_SCOPE
    PUBLIC
  )
else()
  message(
    STATUS
    "[realtime] Building INTERFACE library because src/ is empty"
  )

  add_library(
    vix_realtime
    INTERFACE
  )

  set(
    VIX_REALTIME_LINK_SCOPE
    INTERFACE
  )
endif()

if(NOT TARGET vix::realtime)
  add_library(
    vix::realtime
    ALIAS
    vix_realtime
  )
endif()

target_compile_features(
  vix_realtime
  ${VIX_REALTIME_LINK_SCOPE}
  cxx_std_20
)

target_include_directories(
  vix_realtime
  ${VIX_REALTIME_LINK_SCOPE}
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(
  vix_realtime
  ${VIX_REALTIME_LINK_SCOPE}
  ${VIX_REALTIME_CORE_TARGET}
  ${VIX_REALTIME_ERROR_TARGET}
  ${VIX_REALTIME_JSON_TARGET}
  ${VIX_REALTIME_ASYNC_TARGET}
  ${VIX_REALTIME_SYNC_TARGET}
  ${VIX_REALTIME_TIME_TARGET}
  ${VIX_REALTIME_UTILS_TARGET}
  Threads::Threads
)

if(VIX_REALTIME_WITH_WEBSOCKET)
  target_link_libraries(
    vix_realtime
    ${VIX_REALTIME_LINK_SCOPE}
    ${VIX_REALTIME_WEBSOCKET_TARGET}
  )
endif()

if(VIX_REALTIME_WITH_POSTGRES)
  target_link_libraries(
    vix_realtime
    ${VIX_REALTIME_LINK_SCOPE}
    ${VIX_REALTIME_DB_TARGET}
  )
endif()

target_compile_definitions(
  vix_realtime
  ${VIX_REALTIME_LINK_SCOPE}
  VIX_REALTIME_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  VIX_REALTIME_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  VIX_REALTIME_VERSION_PATCH=${PROJECT_VERSION_PATCH}
  VIX_REALTIME_WITH_WEBSOCKET=$<BOOL:${VIX_REALTIME_WITH_WEBSOCKET}>
  VIX_REALTIME_WITH_POSTGRES=$<BOOL:${VIX_REALTIME_WITH_POSTGRES}>
)

if(REALTIME_SOURCES)
  if(MSVC)
    target_compile_options(
      vix_realtime
      PRIVATE
      /W4
      /permissive-
    )
  else()
    target_compile_options(
      vix_realtime
      PRIVATE
      -Wall
      -Wextra
      -Wpedantic
    )
  endif()

  if(VIX_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(
      vix_realtime
      PUBLIC
      -fno-omit-frame-pointer
      -fsanitize=address,undefined
    )

    target_link_options(
      vix_realtime
      INTERFACE
      -fsanitize=address,undefined
    )
  endif()
endif()

set_target_properties(
  vix_realtime
  PROPERTIES
    OUTPUT_NAME vix_realtime
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME realtime
)

# --------------------------------------------------------------------
# Installation
# --------------------------------------------------------------------

if(VIX_REALTIME_ENABLE_INSTALL)
  set(
    VIX_REALTIME_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/vix_realtime"
  )

  if(NOT VIX_UMBRELLA_BUILD)
    configure_package_config_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vix_realtimeConfig.cmake.in"
      "${CMAKE_CURRENT_BINARY_DIR}/vix_realtimeConfig.cmake"
      INSTALL_DESTINATION
        "${VIX_REALTIME_INSTALL_CMAKEDIR}"
      NO_SET_AND_CHECK_MACRO
    )

    write_basic_package_version_file(
      "${CMAKE_CURRENT_BINARY_DIR}/vix_realtimeConfigVersion.cmake"
      VERSION
        ${PROJECT_VERSION}
      COMPATIBILITY
        SameMajorVersion
    )
  endif()

  install(
    TARGETS
      vix_realtime
    EXPORT
      ${VIX_REALTIME_EXPORT_SET}
    ARCHIVE DESTINATION
      ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION
      ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION
      ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION
      ${CMAKE_INSTALL_INCLUDEDIR}
  )

  if(NOT VIX_UMBRELLA_BUILD)
    install(
      EXPORT
        VixRealtimeTargets
      FILE
        VixRealtimeTargets.cmake
      NAMESPACE
        vix::
      DESTINATION
        "${VIX_REALTIME_INSTALL_CMAKEDIR}"
    )
  endif()

  if(NOT VIX_UMBRELLA_BUILD)
    install(
      FILES
        "${CMAKE_CURRENT_BINARY_DIR}/vix_realtimeConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/vix_realtimeConfigVersion.cmake"
      DESTINATION
        "${VIX_REALTIME_INSTALL_CMAKEDIR}"
    )
  endif()

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
    install(
      DIRECTORY
        include/
      DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
      FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h"
        PATTERN "*.inc"
    )
  endif()
endif()

# --------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------

if(VIX_REALTIME_BUILD_TESTS)
  include(CTest)

  enable_testing()

  add_subdirectory(
    tests
  )
endif()

# --------------------------------------------------------------------
# Examples
# --------------------------------------------------------------------

if(
  VIX_REALTIME_BUILD_EXAMPLES
  AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt"
)
  add_subdirectory(
    examples
  )
endif()

# --------------------------------------------------------------------
# Configuration summary
# --------------------------------------------------------------------

message(
  STATUS
  "------------------------------------------------------"
)

message(
  STATUS
  "vix::realtime configured (${PROJECT_VERSION})"
)

if(REALTIME_SOURCES)
  message(
    STATUS
    "Mode:                  STATIC"
  )
else()
  message(
    STATUS
    "Mode:                  INTERFACE"
  )
endif()

message(
  STATUS
  "WebSocket adapter:     ${VIX_REALTIME_WITH_WEBSOCKET}"
)

message(
  STATUS
  "PostgreSQL stores:     ${VIX_REALTIME_WITH_POSTGRES}"
)

message(
  STATUS
  "Build tests:           ${VIX_REALTIME_BUILD_TESTS}"
)

message(
  STATUS
  "Build examples:        ${VIX_REALTIME_BUILD_EXAMPLES}"
)

message(
  STATUS
  "Install package:       ${VIX_REALTIME_ENABLE_INSTALL}"
)

message(
  STATUS
  "Fetch dependencies:    ${VIX_REALTIME_FETCH_DEPENDENCIES}"
)

message(
  STATUS
  "Include directory:     ${CMAKE_CURRENT_SOURCE_DIR}/include"
)

message(
  STATUS
  "Build type:            ${CMAKE_BUILD_TYPE}"
)

message(
  STATUS
  "------------------------------------------------------"
)
