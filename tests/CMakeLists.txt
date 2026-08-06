# ====================================================================
#
#  Vix.cpp — Realtime module tests
#
#  Every *_test.cpp file is compiled as an independent executable.
#
#  Example:
#
#    tests/types/room_id_test.cpp
#
#  becomes:
#
#    target: vix_realtime_types_room_id_test
#    CTest:  vix_realtime_types_room_id
#
# ====================================================================

if(NOT TARGET vix::realtime)
  message(
    FATAL_ERROR
    "[realtime/tests] The vix::realtime target is not available"
  )
endif()

find_package(Threads REQUIRED)

if(NOT TARGET GTest::gtest_main)
  find_package(GTest REQUIRED)
endif()

if(NOT TARGET GTest::gtest_main)
  message(
    FATAL_ERROR
    "[realtime/tests] GTest::gtest_main is not available"
  )
endif()

set(
  VIX_REALTIME_TEST_TIMEOUT
  30
  CACHE STRING
  "Default timeout in seconds for realtime tests"
)

file(
  GLOB_RECURSE
  REALTIME_TEST_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp"
)

list(SORT REALTIME_TEST_SOURCES)

if(NOT REALTIME_TEST_SOURCES)
  message(
    STATUS
    "[realtime/tests] No *_test.cpp files found yet"
  )

  return()
endif()

function(vix_realtime_add_test test_source)
  file(
    RELATIVE_PATH
    relative_source
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${test_source}"
  )

  string(REPLACE "\\" "/" relative_source "${relative_source}")

  string(
    REGEX REPLACE
    "_test\\.cpp$"
    ""
    test_stem
    "${relative_source}"
  )

  string(
    REGEX REPLACE
    "[^A-Za-z0-9_]"
    "_"
    target_suffix
    "${test_stem}"
  )

  set(test_target "vix_realtime_${target_suffix}_test")
  set(test_name "vix_realtime_${target_suffix}")

  # PostgreSQL tests only exist when PostgreSQL support is enabled.
  if(
    relative_source MATCHES "^postgres/"
    AND NOT VIX_REALTIME_WITH_POSTGRES
  )
    message(
      STATUS
      "[realtime/tests] Skipping ${relative_source}: "
      "PostgreSQL support is disabled"
    )

    return()
  endif()

  # WebSocket transport tests require the transport adapter.
  if(
    relative_source MATCHES "^transport/"
    AND NOT VIX_REALTIME_WITH_WEBSOCKET
  )
    message(
      STATUS
      "[realtime/tests] Skipping ${relative_source}: "
      "WebSocket support is disabled"
    )

    return()
  endif()

  add_executable(
    ${test_target}
    "${test_source}"
  )

  target_compile_features(
    ${test_target}
    PRIVATE
    cxx_std_20
  )

  target_compile_definitions(
    ${test_target}
    PRIVATE
    VIX_REALTIME_TESTING=1
  )

  target_include_directories(
    ${test_target}
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
  )

  target_link_libraries(
    ${test_target}
    PRIVATE
    vix::realtime
    GTest::gtest_main
    Threads::Threads
  )

  if(MSVC)
    target_compile_options(
      ${test_target}
      PRIVATE
      /W4
      /permissive-
    )
  else()
    target_compile_options(
      ${test_target}
      PRIVATE
      -Wall
      -Wextra
      -Wpedantic
    )
  endif()

  add_test(
    NAME ${test_name}
    COMMAND ${test_target}
  )

  get_filename_component(
    test_group
    "${relative_source}"
    DIRECTORY
  )

  if(test_group)
    set_tests_properties(
      ${test_name}
      PROPERTIES
        LABELS "realtime;${test_group}"
        TIMEOUT ${VIX_REALTIME_TEST_TIMEOUT}
    )
  else()
    set_tests_properties(
      ${test_name}
      PROPERTIES
        LABELS "realtime"
        TIMEOUT ${VIX_REALTIME_TEST_TIMEOUT}
    )
  endif()

  message(
    STATUS
    "[realtime/tests] Registered ${test_name}"
  )
endfunction()

foreach(test_source IN LISTS REALTIME_TEST_SOURCES)
  vix_realtime_add_test("${test_source}")
endforeach()
