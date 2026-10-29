find_package(GTest CONFIG REQUIRED)

add_executable(trackpad_tests
  config_test.cc
  geometry_test.cc
  system_test.cc
  tracker_test.cc
)

target_link_libraries(trackpad_tests PRIVATE
  trackpad_core
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(trackpad_tests)
