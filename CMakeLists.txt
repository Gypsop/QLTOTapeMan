cmake_minimum_required(VERSION 3.16)

project(QLTOTapeMan VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the QtWidgets library
find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

set(PROJECT_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/MainWindow.ui
    src/device/DeviceManager.cpp
    src/device/DeviceManager.h
    src/device/LtfsManager.cpp
    src/device/LtfsManager.h
    src/utils/SettingsManager.cpp
    src/utils/SettingsManager.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.ui
    src/ui/AboutDialog.cpp
    src/ui/AboutDialog.h
    src/ui/AboutDialog.ui
    src/ui/FileBrowserWidget.cpp
    src/ui/FileBrowserWidget.h
    src/ui/FileBrowserWidget.ui
    src/ltfs/LtfsElements.h
    src/Version.h
    src/ltfs/LtfsIndexParser.cpp
    src/ltfs/LtfsIndexParser.h
    src/ltfs/IndexManager.cpp
    src/ltfs/IndexManager.h
    src/transfer/RingBuffer.cpp
    src/transfer/RingBuffer.h
    src/transfer/TransferEngine.cpp
    src/transfer/TransferEngine.h
    src/ui/TransferDialog.cpp
    src/ui/TransferDialog.h
)

if(WIN32)
    # WIN32 prevents a console window from opening on Windows
    add_executable(QLTOTapeMan WIN32 ${PROJECT_SOURCES})
    target_link_libraries(QLTOTapeMan PRIVATE Setupapi)
else()
    add_executable(QLTOTapeMan ${PROJECT_SOURCES})
endif()

target_link_libraries(QLTOTapeMan PRIVATE Qt6::Widgets Qt6::Concurrent)

# Handle UI files, MOC, and Resources automatically
set_target_properties(QLTOTapeMan PROPERTIES
    AUTOUIC ON
    AUTOMOC ON
    AUTORCC ON
)
