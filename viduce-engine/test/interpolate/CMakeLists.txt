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

add_executable(shm_frame_packer_test shm_frame_packer_test.cc)
target_link_libraries(shm_frame_packer_test
    PRIVATE
    absl::status
    absl::status_matchers
    absl::statusor
    absl::strings
    avutil
    frame
    GTest::gmock
    GTest::gtest_main
    shm_buffer
    shm_frame_packer
)
gtest_discover_tests(shm_frame_packer_test)
