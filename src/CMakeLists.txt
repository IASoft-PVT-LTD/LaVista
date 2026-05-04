add_library(LaVista STATIC
    cpp/internal.cpp
    cpp/window_management.cpp
    cpp/file_dialogs.cpp
)

target_include_directories(LaVista PUBLIC
    $<BUILD_INTERFACE:${LaVista_ROOT}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(LaVista PRIVATE hpp)

if(WIN32)
    target_include_directories(LaVista PRIVATE hpp/platform/win64)
    target_sources(LaVista PRIVATE
        cpp/platform/win64/display_management_win64.cpp
        cpp/platform/win64/webview_management_win64.cpp
        cpp/platform/win64/spa_bundle_loader_win64.cpp
        cpp/platform/win64/window_platform_win64.cpp
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_include_directories(LaVista PRIVATE hpp/platform/linux)
    target_sources(LaVista PRIVATE
        cpp/platform/linux/display_management_linux.cpp
        cpp/platform/linux/spa_bundle_loader_linux.cpp
        cpp/platform/linux/window_platform_linux.cpp
    )
else()
    target_include_directories(LaVista PRIVATE hpp/platform/fallback)
    target_sources(LaVista PRIVATE
        cpp/platform/fallback/window_display_spa_fallback.cpp
    )
endif()

target_link_libraries(LaVista PUBLIC 
    libauxid
    STB_CMake
)

target_link_libraries(LaVista PRIVATE
    NativeFileDialog
    webview::core_static
    auxid_platform_standard
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(X11 REQUIRED)
  target_link_libraries(LaVista PRIVATE X11::X11)
endif()
