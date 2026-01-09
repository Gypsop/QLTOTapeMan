# Core directories
set(QLTO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(QLTO_INCLUDE_DIRS
    ${QLTO_ROOT}
    ${QLTO_ROOT}/..
)

# Core library
set(QLTO_CORE_SOURCES
    core/ltfs_index.cpp
    core/ltfs_label.cpp
)
add_library(qlto_core STATIC ${QLTO_CORE_SOURCES})
target_include_directories(qlto_core PUBLIC ${QLTO_INCLUDE_DIRS})

# IO library (platform toggles handled later)
set(QLTO_IO_SOURCES
        io/tape_factory.cpp
)

if(QLTOTAPEMAN_WITH_LINUX)
    list(APPEND QLTO_IO_SOURCES
        io/tape_device_linux.cpp
        io/tape_enumerator_linux.cpp
    )
endif()

if(QLTOTAPEMAN_WITH_WIN)
    list(APPEND QLTO_IO_SOURCES
        io/tape_device_win.cpp
        io/tape_enumerator_win.cpp
    )
endif()

if(QLTOTAPEMAN_WITH_MAC)
    list(APPEND QLTO_IO_SOURCES
        io/tape_device_mac.cpp
        io/tape_enumerator_mac.cpp
    )
endif()

add_library(qlto_io STATIC ${QLTO_IO_SOURCES})
target_include_directories(qlto_io PUBLIC ${QLTO_INCLUDE_DIRS})

if(QLTOTAPEMAN_WITH_MAC)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    target_link_libraries(qlto_io PUBLIC ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
endif()

target_compile_definitions(qlto_io PUBLIC
    $<$<BOOL:${QLTOTAPEMAN_WITH_WIN}>:QLTO_PLATFORM_WIN>
    $<$<BOOL:${QLTOTAPEMAN_WITH_LINUX}>:QLTO_PLATFORM_LINUX>
    $<$<BOOL:${QLTOTAPEMAN_WITH_MAC}>:QLTO_PLATFORM_MAC>
)

# Pipeline library
set(QLTO_PIPELINE_SOURCES
    pipeline/file_pipeline_stub.cpp
)
add_library(qlto_pipeline STATIC ${QLTO_PIPELINE_SOURCES})
target_include_directories(qlto_pipeline PUBLIC ${QLTO_INCLUDE_DIRS})

# UI placeholder interface (no QML, only Widgets later)
add_library(qlto_ui INTERFACE)
target_include_directories(qlto_ui INTERFACE ${QLTO_INCLUDE_DIRS})

# Main executable
add_executable(qltotapeman app/main.cpp)
target_link_libraries(qltotapeman PRIVATE
    qlto_core
    qlto_io
    qlto_pipeline
    qlto_ui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
)

target_include_directories(qltotapeman PRIVATE ${QLTO_INCLUDE_DIRS})

target_compile_definitions(qltotapeman PRIVATE
    $<$<BOOL:${QLTOTAPEMAN_WITH_WIN}>:QLTO_PLATFORM_WIN>
    $<$<BOOL:${QLTOTAPEMAN_WITH_LINUX}>:QLTO_PLATFORM_LINUX>
    $<$<BOOL:${QLTOTAPEMAN_WITH_MAC}>:QLTO_PLATFORM_MAC>
)
