include(ExternalProject)

# Add more flags / options based on previous cmake flags
ExternalProject_Add(ffmpeg
    GIT_REPOSITORY https://git.ffmpeg.org/ffmpeg.git
    GIT_TAG n8.0.1
    CONFIGURE_COMMAND "./configure" "--disable-programs" "--disable-doc" "--enable-pic" "--enable-shared"
    BUILD_IN_SOURCE ON
    BUILD_COMMAND "make" "-j8"
    INSTALL_COMMAND ""
)
ExternalProject_Get_Property(ffmpeg SOURCE_DIR)

add_library(libavcodec SHARED IMPORTED GLOBAL)
set_property(TARGET libavcodec
    PROPERTY IMPORTED_LOCATION 
    "${SOURCE_DIR}/libavcodec/libavcodec.so"
)
target_include_directories(libavcodec INTERFACE ${SOURCE_DIR})
target_link_libraries(libavcodec INTERFACE libswresample)
add_dependencies(libavcodec ffmpeg)

add_library(libavdevice SHARED IMPORTED GLOBAL)
set_property(TARGET libavdevice
    PROPERTY IMPORTED_LOCATION 
    "${SOURCE_DIR}/libavdevice/libavdevice.so"
)
target_include_directories(libavdevice INTERFACE ${SOURCE_DIR})
add_dependencies(libavdevice ffmpeg)

add_library(libavfilter SHARED IMPORTED GLOBAL)
set_property(TARGET libavfilter
    PROPERTY IMPORTED_LOCATION 
    "${SOURCE_DIR}/libavfilter/libavfilter.so"
)
target_include_directories(libavfilter INTERFACE ${SOURCE_DIR})
add_dependencies(libavfilter ffmpeg)

add_library(libavformat SHARED IMPORTED GLOBAL)
set_property(TARGET libavformat
    PROPERTY IMPORTED_LOCATION 
    "${SOURCE_DIR}/libavformat/libavformat.so"
)
target_include_directories(libavformat INTERFACE ${SOURCE_DIR})
add_dependencies(libavformat ffmpeg)

add_library(libavutil SHARED IMPORTED GLOBAL)
set_property(TARGET libavutil
    PROPERTY IMPORTED_LOCATION 
    "${SOURCE_DIR}/libavutil/libavutil.so"
)
target_include_directories(libavutil INTERFACE ${SOURCE_DIR})
add_dependencies(libavutil ffmpeg)

add_library(libswresample SHARED IMPORTED GLOBAL)
set_property(TARGET libswresample
    PROPERTY IMPORTED_LOCATION 
    "${SOURCE_DIR}/libswresample/libswresample.so"
)
target_include_directories(libswresample INTERFACE ${SOURCE_DIR})
add_dependencies(libswresample ffmpeg)

add_library(libswscale SHARED IMPORTED GLOBAL)
set_property(TARGET libswscale
    PROPERTY IMPORTED_LOCATION 
    "${SOURCE_DIR}/libswscale/libswscale.so"
)
target_include_directories(libswscale INTERFACE ${SOURCE_DIR})
add_dependencies(libswscale ffmpeg)
