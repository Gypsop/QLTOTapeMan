# QLTOTapeMan - Qt-based LTO Tape Manager
# Application CMakeLists.txt
#
# Copyright (c) 2026 Jeffrey ZHU (zhxsh1225@gmail.com)
# https://github.com/Gypsop/QLTOTapeMan

cmake_minimum_required(VERSION 3.16)

# Find Qt components
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
    LinguistTools
    Charts
)

set(APP_SOURCES
    main.cpp
    gui/MainWindow.cpp
    gui/MainWindow.h
    gui/FileBrowserDialog.cpp
    gui/FileBrowserDialog.h
    gui/LtfsWriterWindow.cpp
    gui/LtfsWriterWindow.h
    gui/DeviceSelectWidget.cpp
    gui/DeviceSelectWidget.h
    gui/ProgressDialog.cpp
    gui/ProgressDialog.h
    gui/SettingsDialog.cpp
    gui/SettingsDialog.h
    gui/AboutDialog.cpp
    gui/AboutDialog.h
)

set(APP_UI_FILES
    gui/MainWindow.ui
    gui/FileBrowserDialog.ui
    gui/LtfsWriterWindow.ui
    gui/DeviceSelectWidget.ui
    gui/ProgressDialog.ui
    gui/SettingsDialog.ui
    gui/AboutDialog.ui
)

set(APP_RESOURCES
    resources/resources.qrc
)

# Translation files
set(TS_FILES
    translations/QLTOTapeMan_en.ts
    translations/QLTOTapeMan_zh_CN.ts
    translations/QLTOTapeMan_zh_TW.ts
)

# Create translations
qt6_add_translation(QM_FILES ${TS_FILES})

# Create executable
if(WIN32)
    # Windows: GUI application (no console)
    add_executable(${PROJECT_NAME} WIN32
        ${APP_SOURCES}
        ${APP_UI_FILES}
        ${APP_RESOURCES}
        ${QM_FILES}
        resources/app.rc
    )
else()
    add_executable(${PROJECT_NAME}
        ${APP_SOURCES}
        ${APP_UI_FILES}
        ${APP_RESOURCES}
        ${QM_FILES}
    )
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gui
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    libqltfs
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Charts
)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Copy translation files to output
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin/translations
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QM_FILES} ${CMAKE_BINARY_DIR}/bin/translations
)

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(FILES ${QM_FILES}
    DESTINATION share/${PROJECT_NAME}/translations
)

# Desktop file for Linux
if(UNIX AND NOT APPLE)
    install(FILES resources/QLTOTapeMan.desktop
        DESTINATION share/applications
    )
    install(FILES resources/icons/QLTOTapeMan.png
        DESTINATION share/icons/hicolor/256x256/apps
    )
endif()
