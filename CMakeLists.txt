cmake_minimum_required(VERSION 3.20)
project(QLTOTapeMan VERSION 0.1.0 LANGUAGES CXX)

# Options
option(QLTOTAPEMAN_BUILD_TESTS "Build tests" OFF)
option(QLTOTAPEMAN_USE_SYSTEM_BLAKE3 "Use system Blake3" OFF)
option(QLTOTAPEMAN_ENABLE_ZSTD "Enable Zstd compression" OFF)

if(NOT DEFINED QLTOTAPEMAN_WITH_WIN)
  if(WIN32)
    set(QLTOTAPEMAN_WITH_WIN ON)
  else()
    set(QLTOTAPEMAN_WITH_WIN OFF)
  endif()
endif()

if(NOT DEFINED QLTOTAPEMAN_WITH_LINUX)
  if(UNIX AND NOT APPLE)
    set(QLTOTAPEMAN_WITH_LINUX ON)
  else()
    set(QLTOTAPEMAN_WITH_LINUX OFF)
  endif()
endif()

if(NOT DEFINED QLTOTAPEMAN_WITH_MAC)
  if(APPLE)
    set(QLTOTAPEMAN_WITH_MAC ON)
  else()
    set(QLTOTAPEMAN_WITH_MAC OFF)
  endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Warnings
if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Qt6 6.0 COMPONENTS Widgets Network Concurrent REQUIRED)

add_subdirectory(src)

if(QLTOTAPEMAN_BUILD_TESTS)
  enable_testing()
  if(EXISTS ${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
  endif()
endif()
