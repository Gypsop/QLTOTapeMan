#
# QLTOTapeMan - Qt-based LTO Tape Manager
#
# Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
# https://github.com/Gypsop/QLTOTapeMan
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

cmake_minimum_required(VERSION 3.16)

project(QLTOTapeMan
    VERSION 1.0.0
    DESCRIPTION "Qt-based LTO Tape Manager with LTFS support"
    HOMEPAGE_URL "https://github.com/Gypsop/QLTOTapeMan"
    LANGUAGES CXX
)

# =============================================================================
# C++ Standard Configuration
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Qt Configuration
# =============================================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 - require at least version 6.0
find_package(Qt6 6.0 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Xml
    Concurrent
    LinguistTools
)

# =============================================================================
# Platform Detection
# =============================================================================
if(WIN32)
    set(QLTFS_PLATFORM_WINDOWS TRUE)
    add_definitions(-DQLTFS_PLATFORM_WINDOWS)
    message(STATUS "Platform: Windows")
elseif(UNIX AND NOT APPLE)
    set(QLTFS_PLATFORM_LINUX TRUE)
    add_definitions(-DQLTFS_PLATFORM_LINUX)
    message(STATUS "Platform: Linux")
else()
    message(WARNING "Unsupported platform. Only Windows and Linux are supported.")
endif()

# =============================================================================
# Compiler Configuration
# =============================================================================
if(MSVC)
    # MSVC specific flags
    add_compile_options(
        /W4                 # Warning level 4
        /utf-8              # UTF-8 source and execution charset
        /permissive-        # Strict conformance mode
        /Zc:__cplusplus     # Correct __cplusplus macro
        /MP                 # Multi-processor compilation
    )
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DUNICODE -D_UNICODE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC/Clang specific flags
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_compile_options(-Wno-deprecated-copy)
    endif()
endif()

# =============================================================================
# Build Type Configuration
# =============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo"
)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# =============================================================================
# Output Directories
# =============================================================================
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# Global Include Directories
# =============================================================================
include_directories(${CMAKE_SOURCE_DIR}/src)

# =============================================================================
# Subdirectories
# =============================================================================
add_subdirectory(src/libqltfs)
add_subdirectory(src/app)

# =============================================================================
# Installation Configuration
# =============================================================================
include(GNUInstallDirs)

# Install license and documentation
install(FILES
    LICENSE
    README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

# =============================================================================
# CPack Configuration for Packaging
# =============================================================================
set(CPACK_PACKAGE_NAME "QLTOTapeMan")
set(CPACK_PACKAGE_VENDOR "Jeffrey ZHU")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt-based LTO Tape Manager with LTFS support")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jeffrey ZHU <zhxsh1225@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6-core6, libqt6-gui6, libqt6-widgets6, libqt6-xml6")
endif()

include(CPack)

# =============================================================================
# Summary
# =============================================================================
message(STATUS "")
message(STATUS "QLTOTapeMan Configuration Summary")
message(STATUS "==================================")
message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "Qt Version:     ${Qt6_VERSION}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
