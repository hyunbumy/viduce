include(GoogleTest)

file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(engine_api_test engine_api_test.cc)
target_link_libraries(engine_api_test PRIVATE absl::status gtest_main)
target_link_libraries(engine_api_test PRIVATE engine_api)
gtest_discover_tests(engine_api_test)

add_executable(frame_reader_test frame_reader_test.cc)
target_link_libraries(frame_reader_test PRIVATE absl::status gtest_main)
target_link_libraries(frame_reader_test PRIVATE frame_reader)
gtest_discover_tests(frame_reader_test)
