add_executable(upscaler_test upscaler_test.cc)
target_link_libraries(upscaler_test
    PRIVATE
    absl::status
    absl::status_matchers
    avutil
    GTest::gmock
    GTest::gtest_main
)
target_link_libraries(upscaler_test PRIVATE frame frame_reader model_impl upscaler)
gtest_discover_tests(upscaler_test)
