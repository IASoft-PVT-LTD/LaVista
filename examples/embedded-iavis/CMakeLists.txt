add_executable(EmbeddedIAVis
  cpp/main.cpp
)

if(WIN32)
  set_property(TARGET EmbeddedIAVis PROPERTY WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(EmbeddedIAVis PRIVATE
  IAVis
  LaVista
  auxid_platform_standard
  glm::glm
)

if(MSVC)
  target_link_options(EmbeddedIAVis PRIVATE /FORCE:MULTIPLE)
endif()
