cmake_minimum_required(VERSION 3.16)

project(sheaf
    VERSION 0.0.1
    DESCRIPTION "Document library and manager for recent and favorite files"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)

option(SHEAF_BUILD_TESTS "Build the unit tests" ON)
option(SHEAF_WITH_GIO "Read reading progress metadata through GIO when available" ON)
option(SHEAF_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Sql Concurrent Network DBus Test)
find_package(Qt6 OPTIONAL_COMPONENTS LinguistTools)

set(SHEAF_HAVE_GIO OFF)
if(SHEAF_WITH_GIO)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GIO IMPORTED_TARGET gio-2.0)
        if(GIO_FOUND)
            set(SHEAF_HAVE_GIO ON)
        endif()
    endif()
endif()
message(STATUS "Sheaf: Qt ${Qt6_VERSION}, GIO progress support: ${SHEAF_HAVE_GIO}")

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
if(SHEAF_WARNINGS_AS_ERRORS)
    add_compile_options(-Werror)
endif()
add_compile_definitions(QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_BYTEARRAY QT_USE_QSTRINGBUILDER)

if(SHEAF_HAVE_GIO)
    set(SHEAF_HAVE_GIO_INT 1)
else()
    set(SHEAF_HAVE_GIO_INT 0)
endif()
configure_file(src/core/version.h.in ${CMAKE_BINARY_DIR}/generated/sheaf/version.h @ONLY)

add_subdirectory(src)

if(SHEAF_BUILD_TESTS AND EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()

add_subdirectory(data)
