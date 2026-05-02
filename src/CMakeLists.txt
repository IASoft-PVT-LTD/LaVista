add_library(LaVista STATIC cpp/LaVista.cpp)

target_include_directories(LaVista PUBLIC
    $<BUILD_INTERFACE:${LaVista_ROOT}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(LaVista PRIVATE hpp)

target_link_libraries(LaVista PUBLIC 
    libauxid
)

target_link_libraries(LaVista PRIVATE 
    webview::core_static
    auxid_platform_standard
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(X11 REQUIRED)
  target_link_libraries(LaVista PRIVATE X11::X11)
endif()
