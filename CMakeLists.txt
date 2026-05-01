cmake_minimum_required(VERSION 3.20)

project(LaVista)
enable_testing()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(LaVista_IS_TOP_LEVEL ON)
else()
    set(LaVista_IS_TOP_LEVEL OFF)
endif()

set(LaVista_ROOT "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "")

if(TARGET libauxid)
    message(STATUS "LaVista: Using external LibAuxid provided by parent project")
else()
    message(STATUS "LaVista: Building standalone, using local LibAuxid submodule")
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libauxid/CMakeLists.txt")
        message(FATAL_ERROR "LibAuxid submodule missing! Run: git submodule update --init --recursive")
    endif()
    add_subdirectory(libauxid)
endif()

auxid_setup_project()

option(LaVista_BUILD_TESTS "Build unit tests" ${LaVista_IS_TOP_LEVEL})

include(cmake/find_deps.cmake)

add_subdirectory(src)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sandbox")
    add_subdirectory(sandbox)
endif()

if(LaVista_BUILD_TESTS)
    add_subdirectory(tests)
endif()
