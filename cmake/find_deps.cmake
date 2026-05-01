include(FetchContent)

FetchContent_Declare(
  webview
  GIT_REPOSITORY https://github.com/webview/webview
  GIT_TAG 0.12.0
  SYSTEM
)
set(WEBVIEW_BUILD_DOCS OFF)
set(WEBVIEW_BUILD_TESTS OFF)
set(WEBVIEW_BUILD_EXAMPLES OFF)
set(WEBVIEW_BUILD_STATIC_LIBRARY ON)
set(WEBVIEW_BUILD_SHARED_LIBRARY OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(WEBVIEW_WEBKITGTK_API 6.0)
endif()
FetchContent_MakeAvailable(webview)
