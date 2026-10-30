add_executable(shm_buffer_test shm_buffer_test.cc)
target_link_libraries(shm_buffer_test
    PRIVATE
    absl::status
    absl::status_matchers
    absl::statusor
    absl::strings
    GTest::gmock
    GTest::gtest_main
    shm_buffer
)
gtest_discover_tests(shm_buffer_test)
