add_library(LaVista STATIC cpp/LaVista.cpp)

target_include_directories(LaVista PUBLIC
    $<BUILD_INTERFACE:${LaVista_ROOT}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(LaVista PRIVATE hpp)

target_link_libraries(LaVista PUBLIC 
    libauxid
    webview::core_static
)
target_link_libraries(LaVista PRIVATE 
    auxid_platform_standard
)
