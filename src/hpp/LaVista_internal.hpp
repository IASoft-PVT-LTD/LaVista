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
#include <auxid/memory/box.hpp>

#define WEBVIEW_HEADER
#include <webview/webview.h>
#undef WEBVIEW_HEADER

#include <LaVista_platform_context.hpp>

namespace LaVista
{
  namespace _internal
  {
    struct DragBindCtx
    {
      Window window = nullptr;
      webview_t sender = nullptr;
    };

    struct ChromeBindCtx
    {
      Window window = nullptr;
      webview_t sender = nullptr;
    };
  } // namespace _internal

  struct BindingContext_T
  {
    Window window = nullptr;
    String event;
  };

  struct JsonBindingContext_T
  {
    Window window = nullptr;
    String name;
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
    WindowDragStripOptions drag_strip_backup{};
    bool drag_strip_backup_valid = false;
    bool content_drag_strip_js_installed = false;
    bool titlebar_drag_strip_js_installed = false;
    webview_t titlebar_webview = nullptr;
    i32 titlebar_height_px = 0;
    String titlebar_html{};
    bool running = true;
    HashMap<String, Function<void, const String &>> callbacks;
    Function<void> menu_button_callback;
    bool menu_button_callback_bound = false;
    HashMap<String, memory::Box<BindingContext_T>> binding_contexts;
    HashMap<String, Function<String, const String &>> json_binding_handlers;
    HashMap<String, memory::Box<JsonBindingContext_T>> json_binding_contexts;
    HashMap<String, Vec<u8>> pending_binary_buffers;
    u64 next_binary_buffer_id = 0;
    memory::Box<_internal::DragBindCtx> content_drag_bind_ctx{nullptr};
    memory::Box<_internal::DragBindCtx> titlebar_drag_bind_ctx{nullptr};
    memory::Box<_internal::ChromeBindCtx> chrome_bind_ctx{nullptr};
    PlatformWindowContext platform{};
    Vec<filesystem::Path> temp_files;
  };

  namespace _internal
  {
    inline auto webview_error_to_result(webview_error_t err, const char *context) -> Result<void>
    {
      if (WEBVIEW_SUCCEEDED(err))
      {
        return {};
      }
      return fail("%s failed with webview error code %d", context, static_cast<int>(err));
    }

    auto platform_get_displays() -> Result<Vec<DisplayInfo>>;

    auto platform_create_window(Window_T &state, i32 width, i32 height, i32 window_x, i32 window_y, const String &title,
                                const String &icon_path) -> Result<void>;

    auto platform_apply_post_webview_setup(Window_T &state, i32 width, i32 height) -> Result<void>;

    auto platform_destroy_native(Window window) -> void;

    /* Returns false when the native window is no longer valid. */
    auto platform_pump_events(Window window) -> bool;

    auto platform_sync_window_frame_from_native(Window window) -> void;

    auto platform_set_window_position(Window window, i32 x, i32 y) -> Result<void>;

    auto platform_set_window_size(Window window, i32 width, i32 height) -> Result<void>;

    auto platform_start_window_drag(Window window) -> void;

    auto platform_minimize_window(Window window) -> void;
    auto platform_toggle_maximize_window(Window window) -> void;
    auto platform_window_is_maximized(Window window) -> bool;
    auto platform_close_window(Window window) -> void;

    auto platform_create_titlebar_webview(Window window) -> Result<void>;
    auto platform_destroy_titlebar_webview(Window window) -> Result<void>;
    auto platform_layout_webviews(Window window) -> void;

    auto load_spa_bundle_into_webview(Window window, webview_t w, const filesystem::Path &index_html,
                                      const filesystem::Path &bundle_dir_abs) -> Result<void>;

    auto load_inline_html_into_webview(Window window, webview_t w, String html_document) -> Result<void>;

    auto build_default_titlebar_html(const String &window_title, const String &icon_path) -> Result<String>;
  } // namespace _internal

  namespace utils
  {
    auto load_image_rgba_from_file(const String &path, Vec<u8> &out_rgba, i32 &out_w, i32 &out_h) -> Result<void>;

    auto resize_rgba_nearest(const u8 *src, i32 sw, i32 sh, i32 dw, i32 dh, Vec<u8> &out) -> void;

    auto load_spa_bundle_file_scheme(Window window, webview_t w, const filesystem::Path &index_html,
                                     const filesystem::Path &bundle_dir_abs) -> Result<void>;
  } // namespace utils
} // namespace LaVista
