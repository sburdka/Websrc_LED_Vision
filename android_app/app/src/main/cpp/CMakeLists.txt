cmake_minimum_required(VERSION 3.22.1)
project("visionkiosk")

# Download cpp-httplib (header-only, no external deps needed)
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.15.3
)
FetchContent_MakeAvailable(httplib)

add_library(
    visionkiosk
    SHARED
    server.cpp
)

target_include_directories(visionkiosk PRIVATE
    ${httplib_SOURCE_DIR}
)

target_compile_options(visionkiosk PRIVATE
    -std=c++17
    -O2
    -fexceptions
    -frtti
)

# Link Android system libraries
target_link_libraries(visionkiosk
    android
    log
    OpenSLES      # Audio beep generation
)
