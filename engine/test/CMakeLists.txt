include(GoogleTest)

file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(engine_api_test engine_api_test.cc)
target_link_libraries(engine_api_test PRIVATE absl::status engine_api gtest_main)

gtest_discover_tests(engine_api_test)
