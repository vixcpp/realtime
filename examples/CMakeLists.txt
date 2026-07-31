# ====================================================================
#
#  Vix.cpp — Realtime examples
#
# ====================================================================

set(VIX_REALTIME_EXAMPLE_DIRECTORIES
  counter
  shared_room
  chat
  reconnect
)

foreach(example_directory IN LISTS VIX_REALTIME_EXAMPLE_DIRECTORIES)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${example_directory}/CMakeLists.txt")
    add_subdirectory("${example_directory}")
  else()
    message(
      WARNING
      "[realtime] Example directory is missing: ${example_directory}"
    )
  endif()
endforeach()
