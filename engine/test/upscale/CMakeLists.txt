add_executable(upscaler_test upscaler_test.cc)
target_link_libraries(upscaler_test
    PRIVATE
    absl::status
    avutil
    gmock
    gtest_main
    litert
)
target_link_libraries(upscaler_test PRIVATE frame frame_reader upscaler)
gtest_discover_tests(upscaler_test)
