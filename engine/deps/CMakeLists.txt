# External dependencies for viduce_engine

include(FetchContent)

include(spdlog.cmake)

FetchContent_Declare(abseil
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG 20260107.0
)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

if (BUILD_TESTING)
    set(ABSL_BUILD_TEST_HELPERS ON)
    set(ABSL_USE_EXTERNAL_GOOGLETEST ON)
    set(ABSL_FIND_GOOGLETEST OFF)
    FetchContent_MakeAvailable(googletest)
endif()

FetchContent_MakeAvailable(abseil)

include(litert.cmake)
