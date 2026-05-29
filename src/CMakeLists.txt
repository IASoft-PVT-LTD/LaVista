add_library(LaVista STATIC
    cpp/internal.cpp
    cpp/internal_internal.cpp
    cpp/window_management.cpp
    cpp/file_dialogs.cpp
)

target_sources(LaVista
    PUBLIC FILE_SET lavista_modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_LIST_DIR}/modules"
        FILES
            "${CMAKE_CURRENT_LIST_DIR}/modules/lavista.definitions.cppm"
            "${CMAKE_CURRENT_LIST_DIR}/modules/lavista.internal.cppm"
            "${CMAKE_CURRENT_LIST_DIR}/modules/lavista.cppm"
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
)

set_target_properties(LaVista PROPERTIES CXX_SCAN_FOR_MODULES ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(X11 REQUIRED)
  target_link_libraries(LaVista PRIVATE X11::X11)
  if(LaVista_GTK4_CFLAGS_OTHER)
    target_compile_options(LaVista PUBLIC ${LaVista_GTK4_CFLAGS_OTHER})
  endif()
endif()
