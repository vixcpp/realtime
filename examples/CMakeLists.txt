cmake_minimum_required(VERSION 3.20)

project(vix_example_realtime LANGUAGES CXX)

find_package(Vix CONFIG REQUIRED)

set(_vix_realtime_examples chat counter reconnect shared_room)

enable_testing()

foreach (_example IN LISTS _vix_realtime_examples)
  add_executable("realtime_${_example}" "${_example}/main.cpp")
  target_compile_features("realtime_${_example}" PRIVATE cxx_std_20)
  target_link_libraries("realtime_${_example}" PRIVATE vix::realtime)
  add_test(NAME "realtime.${_example}" COMMAND "realtime_${_example}")
endforeach()
