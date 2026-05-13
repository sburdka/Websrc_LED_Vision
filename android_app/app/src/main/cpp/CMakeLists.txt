cmake_minimum_required(VERSION 3.22.1)
project("visionkiosk")

# ── Locate host Python (needed to embed web assets at build time) ───────────
find_program(PYTHON_EXE NAMES python3 python)
if(NOT PYTHON_EXE)
    message(FATAL_ERROR "Python 3 is required to embed web assets. Install it and re-run CMake.")
endif()

# ── Paths ────────────────────────────────────────────────────────────────────
# From android_app/app/src/main/cpp we go up 5 levels to the repo root
get_filename_component(WEB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

set(GENERATED_ASSETS "${CMAKE_CURRENT_BINARY_DIR}/embedded_files.h")
set(GENERATE_SCRIPT  "${CMAKE_CURRENT_SOURCE_DIR}/generate_assets.py")

# ── Custom command: embed web files into a C++ header at build time ──────────
# The header is regenerated whenever any .html/.js/.css file changes.
file(GLOB_RECURSE WEB_SOURCE_FILES
    "${WEB_ROOT}/*.html"
    "${WEB_ROOT}/*.js"
    "${WEB_ROOT}/*.css"
    "${WEB_ROOT}/*.ttf"
    "${WEB_ROOT}/*.jpg"
    "${WEB_ROOT}/*.jpeg"
    "${WEB_ROOT}/*.png"
    "${WEB_ROOT}/*.gif")

# Strip android_app/ sub-tree from the dependency list (those are not web assets)
list(FILTER WEB_SOURCE_FILES EXCLUDE REGEX ".*/android_app/.*")

add_custom_command(
    OUTPUT  "${GENERATED_ASSETS}"
    COMMAND "${PYTHON_EXE}"
            "${GENERATE_SCRIPT}"
            "${WEB_ROOT}"
            "${GENERATED_ASSETS}"
    DEPENDS "${GENERATE_SCRIPT}" ${WEB_SOURCE_FILES}
    COMMENT "Embedding web assets into C++ binary (no APK assets needed)"
    VERBATIM
)

add_custom_target(generate_assets DEPENDS "${GENERATED_ASSETS}")

# ── cpp-httplib (header-only HTTP server) ─────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.15.3
)
FetchContent_MakeAvailable(httplib)

# ── Native shared library ─────────────────────────────────────────────────────
add_library(visionkiosk SHARED server.cpp)

add_dependencies(visionkiosk generate_assets)

target_include_directories(visionkiosk PRIVATE
    "${httplib_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"   # for generated embedded_files.h
)

target_compile_options(visionkiosk PRIVATE
    -std=c++17
    -O2
    -fexceptions
    -frtti
)

target_link_libraries(visionkiosk
    android
    log
    OpenSLES
)
