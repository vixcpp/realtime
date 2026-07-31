add_executable(vix_realtime_example_chat
  main.cpp
)

target_compile_features(vix_realtime_example_chat
  PRIVATE
    cxx_std_20
)

target_link_libraries(vix_realtime_example_chat
  PRIVATE
    vix::realtime
)
