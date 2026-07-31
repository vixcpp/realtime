add_executable(vix_realtime_example_reconnect
  main.cpp
)

target_compile_features(vix_realtime_example_reconnect
  PRIVATE
    cxx_std_20
)

target_link_libraries(vix_realtime_example_reconnect
  PRIVATE
    vix::realtime
)
