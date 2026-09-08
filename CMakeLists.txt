cmake_minimum_required(VERSION 3.16)
project(screen-recorder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(GLIB REQUIRED glib-2.0 gio-2.0 gio-unix-2.0 gobject-2.0)

set(SOURCES
    src/main.cpp
    src/display_manager.cpp
    src/audio_manager.cpp
    src/settings_manager.cpp
    src/clipboard_helper.cpp
    src/snapshot_helper.cpp
    src/countdown_overlay.cpp
    src/region_selector.cpp
    src/screencast_service.cpp
    src/ui_recorder.cpp
)

add_executable(screen-recorder ${SOURCES})

target_include_directories(screen-recorder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GTK3_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

target_compile_options(screen-recorder PRIVATE
    -Wall
    -Wextra
    ${GTK3_CFLAGS_OTHER}
    ${GLIB_CFLAGS_OTHER}
)

target_link_libraries(screen-recorder PRIVATE
    ${GTK3_LIBRARIES}
    ${GLIB_LIBRARIES}
    pthread
)

# Test executable for device verification
add_executable(verify_devices
    tests/verify_devices.cpp
    src/display_manager.cpp
    src/audio_manager.cpp
    src/settings_manager.cpp
)
target_include_directories(verify_devices PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${GTK3_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS})
target_compile_options(verify_devices PRIVATE -Wall -Wextra ${GTK3_CFLAGS_OTHER} ${GLIB_CFLAGS_OTHER})
target_link_libraries(verify_devices PRIVATE ${GTK3_LIBRARIES} ${GLIB_LIBRARIES} pthread)

# Test executable for end-to-end recording
add_executable(test_record
    tests/test_record.cpp
    src/screencast_service.cpp
    src/audio_manager.cpp
    src/clipboard_helper.cpp
)
target_include_directories(test_record PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${GTK3_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS})
target_compile_options(test_record PRIVATE -Wall -Wextra ${GTK3_CFLAGS_OTHER} ${GLIB_CFLAGS_OTHER})
target_link_libraries(test_record PRIVATE ${GTK3_LIBRARIES} ${GLIB_LIBRARIES} pthread)



# Installation to ~/.local/bin
if(NOT DEFINED CMAKE_INSTALL_PREFIX OR CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Install prefix" FORCE)
endif()

install(TARGETS screen-recorder DESTINATION bin)
