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

#pragma once

#include <LaVista/LaVista.hpp>

#include <auxid/containers/hash_map.hpp>

#define WEBVIEW_HEADER
#include <webview/webview.h>
#undef WEBVIEW_HEADER

#include <LaVista_platform_context.hpp>

#include <filesystem>
#include <functional>
#include <memory>

namespace LaVista
{
  struct BindingContext_T
  {
    Window window = nullptr;
    String event;
  };

  struct Window_T
  {
    webview_t webview = nullptr;
    String title;
    i32 width = 0;
    i32 height = 0;
    i32 x = -1;
    i32 y = -1;
    WindowDragStripOptions drag_strip{};
    bool drag_strip_js_installed = false;
    bool running = true;
    HashMap<String, std::function<void(const String &)>> callbacks;
    HashMap<String, std::unique_ptr<BindingContext_T>> binding_contexts;
    PlatformWindowContext platform{};
  };

  namespace detail
  {
    inline auto webview_error_to_result(webview_error_t err, const char *context) -> Result<void>
    {
      if (WEBVIEW_SUCCEEDED(err))
      {
        return {};
      }
      return au::fail("%s failed with webview error code %d", context, static_cast<int>(err));
    }

    auto platform_get_displays() -> Result<Vec<DisplayInfo>>;

    auto platform_create_window(Window_T &state, i32 width, i32 height, i32 window_x, i32 window_y, const String &title)
        -> Result<void>;

    auto platform_apply_post_webview_setup(Window_T &state, i32 width, i32 height) -> Result<void>;

    void platform_destroy_native(Window window);

    /* Returns false when the native window is no longer valid. */
    bool platform_pump_events(Window window);

    void platform_sync_window_frame_from_native(Window window);

    auto platform_set_window_position(Window window, i32 x, i32 y) -> Result<void>;

    auto platform_set_window_size(Window window, i32 width, i32 height) -> Result<void>;

    void platform_start_window_drag(Window window);

    auto load_spa_bundle_into_webview(webview_t w, const std::filesystem::path &index_html,
                                      const std::filesystem::path &bundle_dir_abs) -> Result<void>;
  } // namespace detail
} // namespace LaVista
