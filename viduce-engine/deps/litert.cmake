set(LITERT_SDK_URL "https://github.com/google-ai-edge/LiteRT/releases/download/v2.1.1/litert_cc_sdk.zip")
FetchContent_Declare(
    litert_cc_sdk
    URL ${LITERT_SDK_URL}
)
# Populate but don't add_subdirectory immediately because we must insert the prebuilt binary first
FetchContent_GetProperties(litert_cc_sdk)
if(NOT litert_cc_sdk_POPULATED)
    message(STATUS "Fetching LiteRT C++ SDK...")
    FetchContent_Populate(litert_cc_sdk)
endif()

# Download the Prebuilt Runtime Library into the SDK Directory
set(LITERT_LIB_URL "https://storage.googleapis.com/litert/binaries/2.1.2/linux_x86_64/libLiteRt.so")
set(LITERT_LIB_DESTINATION "${litert_cc_sdk_SOURCE_DIR}/libLiteRt.so")

if(NOT EXISTS "${LITERT_LIB_DESTINATION}")
    message(STATUS "Downloading prebuilt LiteRT runtime: ${LITERT_LIB_NAME}...")
    file(DOWNLOAD "${LITERT_LIB_URL}" "${LITERT_LIB_DESTINATION}" 
         SHOW_PROGRESS 
         STATUS DOWNLOAD_STATUS
    )

    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        message(WARNING "Failed to download ${LITERT_LIB_NAME}.")
        file(REMOVE "${LITERT_LIB_DESTINATION}")
    endif()
endif()

# Add the SDK source tree to our project
add_subdirectory(${litert_cc_sdk_SOURCE_DIR} ${litert_cc_sdk_BINARY_DIR})
add_library(litert INTERFACE)
target_link_libraries(litert INTERFACE litert_cc_api absl::check absl::flat_hash_set)
target_include_directories(litert INTERFACE ${litert_cc_sdk_SOURCE_DIR} ${CMAKE_BINARY_DIR}/opencl_headers)
