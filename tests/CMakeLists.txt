cmake_minimum_required(VERSION 3.20)
project(MegaMan64Tests LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

# Resolve the repo root whether we are built standalone or via add_subdirectory().
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# ---------------------------------------------------------------------------
# test_yaz0 – unit tests for the Yaz0 decompressor
# ---------------------------------------------------------------------------
add_executable(test_yaz0
    ${CMAKE_CURRENT_SOURCE_DIR}/test_yaz0.cpp
    ${REPO_ROOT}/src/game/rom_decompression.cpp
)

target_include_directories(test_yaz0 PRIVATE
    ${REPO_ROOT}/include
)

add_test(NAME yaz0_decompression COMMAND test_yaz0)
