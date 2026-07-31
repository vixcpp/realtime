add_executable(vix_realtime_example_shared_room
  main.cpp
)

target_compile_features(vix_realtime_example_shared_room
  PRIVATE
    cxx_std_20
)

target_link_libraries(vix_realtime_example_shared_room
  PRIVATE
    vix::realtime
)
