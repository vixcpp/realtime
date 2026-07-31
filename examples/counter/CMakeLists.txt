add_executable(vix_realtime_example_counter
  main.cpp
)

target_compile_features(vix_realtime_example_counter
  PRIVATE
    cxx_std_20
)

target_link_libraries(vix_realtime_example_counter
  PRIVATE
    vix::realtime
)
