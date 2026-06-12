cmake_minimum_required(VERSION 3.16)
project(hft_capture LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(hft_capture INTERFACE)
target_include_directories(hft_capture INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cpp/include)
target_compile_options(hft_capture INTERFACE -Wall -Wextra -Wpedantic -Wconversion)

add_executable(hft_capture_tool cpp/src/capture_tool.cpp)
target_link_libraries(hft_capture_tool PRIVATE hft_capture)

target_compile_options(hft_capture_tool PRIVATE -O3 -march=native)

add_executable(hft_capture_tests cpp/tests/test_capture.cpp)
target_link_libraries(hft_capture_tests PRIVATE hft_capture)

enable_testing()
add_test(NAME hft_capture_tests COMMAND hft_capture_tests)
