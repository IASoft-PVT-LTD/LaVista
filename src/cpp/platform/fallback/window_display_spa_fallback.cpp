// LaVista: A Modern Platform for C++ Desktop Apps.
// Copyright (C) 2026 IAS (ias@iasoft.dev)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <LaVista_internal.hpp>
#include <spa_bundle.hpp>

namespace LaVista::detail
{
  auto platform_get_displays() -> Result<Vec<DisplayInfo>>
  {
    return au::fail("get_displays is not available on this platform");
  }

  auto platform_apply_post_webview_setup(Window_T &state, i32 width, i32 height) -> Result<void>
  {
    return webview_error_to_result(webview_set_size(state.webview, width, height, WEBVIEW_HINT_NONE),
                                   "webview_set_size");
  }

  auto platform_create_window(Window_T &state, i32 width, i32 height, i32 window_x, i32 window_y, const String &title)
      -> Result<void>
  {
    (void) width;
    (void) height;
    (void) window_x;
    (void) window_y;
    (void) title;

    state.webview = webview_create(0, nullptr);
    if (state.webview == nullptr)
    {
      return au::fail("webview_create() failed");
    }
    return {};
  }

  void platform_destroy_native(Window)
  {
  }

  bool platform_pump_events(Window window)
  {
    (void) window;
    return true;
  }

  void platform_sync_window_frame_from_native(Window)
  {
  }

  auto platform_set_window_position(Window, i32, i32) -> Result<void>
  {
    return {};
  }

  auto platform_set_window_size(Window window, i32 width, i32 height) -> Result<void>
  {
    return webview_error_to_result(webview_set_size(window->webview, width, height, WEBVIEW_HINT_NONE),
                                   "webview_set_size");
  }

  void platform_start_window_drag(Window)
  {
  }

  auto load_spa_bundle_into_webview(webview_t w, const std::filesystem::path &index_html,
                                    const std::filesystem::path &bundle_dir_abs) -> Result<void>
  {
    return load_spa_bundle_file_scheme(w, index_html, bundle_dir_abs);
  }
} // namespace LaVista::detail
