#
# QLTOTapeMan - Qt-based LTO Tape Manager
# libqltfs - LTFS Core Library
#
# Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
# https://github.com/Gypsop/QLTOTapeMan
#

# =============================================================================
# Library Sources
# =============================================================================

set(LIBQLTFS_CORE_SOURCES
    core/LtfsTypes.cpp
    core/LtfsIndex.cpp
    core/LtfsLabel.cpp
)

set(LIBQLTFS_CORE_HEADERS
    core/LtfsTypes.h
    core/LtfsIndex.h
    core/LtfsLabel.h
)

set(LIBQLTFS_DEVICE_SOURCES
    device/ScsiCommand.cpp
    device/DeviceEnumerator.cpp
    device/TapeDevice.cpp
)

set(LIBQLTFS_DEVICE_HEADERS
    device/ScsiCommand.h
    device/DeviceEnumerator.h
    device/TapeDevice.h
)

set(LIBQLTFS_IO_SOURCES
    io/TapeIO.cpp
    io/BlockManager.cpp
    io/HashCalculator.cpp
)

set(LIBQLTFS_IO_HEADERS
    io/TapeIO.h
    io/BlockManager.h
    io/HashCalculator.h
)

set(LIBQLTFS_XML_SOURCES
    xml/IndexParser.cpp
    xml/IndexWriter.cpp
)

set(LIBQLTFS_XML_HEADERS
    xml/IndexParser.h
    xml/IndexWriter.h
)

set(LIBQLTFS_UTIL_SOURCES
    util/SizeFormatter.cpp
    util/Logger.cpp
    util/LtfsUtility.cpp
)

set(LIBQLTFS_UTIL_HEADERS
    util/SizeFormatter.h
    util/Logger.h
    util/LtfsUtility.h
)

# Platform-specific sources
if(WIN32)
    set(LIBQLTFS_PLATFORM_SOURCES
        device/platform/WinScsi.cpp
        device/platform/WinDeviceEnumerator.cpp
    )
    set(LIBQLTFS_PLATFORM_HEADERS
        device/platform/WinScsi.h
        device/platform/WinDeviceEnumerator.h
    )
elseif(UNIX AND NOT APPLE)
    set(LIBQLTFS_PLATFORM_SOURCES
        device/platform/LinuxScsi.cpp
        device/platform/LinuxDeviceEnumerator.cpp
    )
    set(LIBQLTFS_PLATFORM_HEADERS
        device/platform/LinuxScsi.h
        device/platform/LinuxDeviceEnumerator.h
    )
endif()

# Combine all sources
set(LIBQLTFS_SOURCES
    ${LIBQLTFS_CORE_SOURCES}
    ${LIBQLTFS_DEVICE_SOURCES}
    ${LIBQLTFS_IO_SOURCES}
    ${LIBQLTFS_XML_SOURCES}
    ${LIBQLTFS_UTIL_SOURCES}
    ${LIBQLTFS_PLATFORM_SOURCES}
)

set(LIBQLTFS_HEADERS
    ${LIBQLTFS_CORE_HEADERS}
    ${LIBQLTFS_DEVICE_HEADERS}
    ${LIBQLTFS_IO_HEADERS}
    ${LIBQLTFS_XML_HEADERS}
    ${LIBQLTFS_UTIL_HEADERS}
    ${LIBQLTFS_PLATFORM_HEADERS}
    libqltfs_global.h
)

# =============================================================================
# Library Target
# =============================================================================

add_library(qltfs STATIC
    ${LIBQLTFS_SOURCES}
    ${LIBQLTFS_HEADERS}
)

# Define export macro for potential future shared library
target_compile_definitions(qltfs PRIVATE LIBQLTFS_LIBRARY)

# Include directories
target_include_directories(qltfs
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/qltfs>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link Qt libraries
target_link_libraries(qltfs
    PUBLIC
        Qt6::Core
        Qt6::Xml
        Qt6::Concurrent
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(qltfs
        PRIVATE
            setupapi
            cfgmgr32
    )
endif()

# =============================================================================
# Installation
# =============================================================================

install(TARGETS qltfs
    EXPORT QLTOTapeManTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers maintaining directory structure
install(FILES libqltfs_global.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qltfs
)

install(FILES ${LIBQLTFS_CORE_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qltfs/core
)

install(FILES ${LIBQLTFS_DEVICE_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qltfs/device
)

install(FILES ${LIBQLTFS_IO_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qltfs/io
)

install(FILES ${LIBQLTFS_XML_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qltfs/xml
)

install(FILES ${LIBQLTFS_UTIL_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qltfs/util
)
