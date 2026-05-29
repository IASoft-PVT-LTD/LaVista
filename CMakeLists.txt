cmake_minimum_required(VERSION 3.28)

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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET libauxid)
    find_package(Threads REQUIRED)
    target_link_libraries(libauxid PUBLIC Threads::Threads)
endif()

auxid_setup_project()

option(LaVista_BUILD_EXAMPLES "Build examples" ${LaVista_IS_TOP_LEVEL})

include(cmake/find_deps.cmake)

add_subdirectory(src)

if(LaVista_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
