cmake_minimum_required(VERSION 3.16)

project(QLTOTapeMan VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/MainWindow.ui
    src/DirectRWDialog.cpp
    src/DirectRWDialog.h
    src/DirectRWDialog.ui
    src/DeviceManager.cpp
    src/DeviceManager.h
    src/TapeModel.cpp
    src/TapeModel.h
    translations/i18n.qrc
)

if(WIN32)
    add_executable(QLTOTapeMan WIN32 ${PROJECT_SOURCES})
else()
    add_executable(QLTOTapeMan ${PROJECT_SOURCES})
endif()

target_link_libraries(QLTOTapeMan PRIVATE Qt6::Widgets)

set_target_properties(QLTOTapeMan PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
