// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

module;

#define WEBVIEW_HEADER
#include <webview/webview.h>
#undef WEBVIEW_HEADER

module lavista.internal;

namespace LaVista::_internal
{
  auto platform_get_displays() -> Result<Vec<DisplayInfo>>
  {
    return fail("get_displays is not available on this platform");
  }

  auto platform_apply_post_webview_setup(Window_T &state, i32 width, i32 height) -> Result<void>
  {
    return webview_error_to_result(webview_set_size(state.webview, width, height, WEBVIEW_HINT_NONE),
                                   "webview_set_size");
  }

  auto platform_create_window(Window_T &state, i32 width, i32 height, i32 window_x, i32 window_y, const String &title,
                              const String &icon_path) -> Result<void>
  {
    (void) width;
    (void) height;
    (void) window_x;
    (void) window_y;
    (void) title;
    (void) icon_path;

    state.webview = webview_create(0, nullptr);
    if (state.webview == nullptr)
    {
      return fail("webview_create() failed");
    }
    return {};
  }

  auto platform_destroy_native(Window) -> void
  {
  }

  auto platform_pump_events(Window window) -> bool
  {
    (void) window;
    return true;
  }

  auto platform_sync_window_frame_from_native(Window) -> void
  {
  }

  auto platform_set_window_position(Window, i32, i32) -> Result<void>
  {
    return {};
  }

  auto platform_set_window_size(Window window, i32 width, i32 height) -> Result<void>
  {
    return webview_error_to_result(webview_set_size(window_ptr(window)->webview, width, height, WEBVIEW_HINT_NONE),
                                   "webview_set_size");
  }

  auto platform_start_window_drag(Window) -> void
  {
  }

  auto platform_minimize_window(Window) -> void
  {
  }

  auto platform_toggle_maximize_window(Window) -> void
  {
  }

  auto platform_window_is_maximized(Window) -> bool
  {
    return false;
  }

  auto platform_close_window(Window) -> void
  {
  }

  auto platform_create_titlebar_webview(Window) -> Result<void>
  {
    return fail("Host-managed title bar is not supported for this platform configuration");
  }

  auto platform_destroy_titlebar_webview(Window window) -> Result<void>
  {
    (void) window;
    return {};
  }

  auto platform_layout_webviews(Window) -> void
  {
  }

  auto platform_post_binary_data(Window, const Span<const u8> &) -> Result<String>
  {
    return fail("post_binary_data is not available on this platform");
  }

  auto load_spa_bundle_into_webview(webview_t w, const filesystem::Path &index_html,
                                    const filesystem::Path &bundle_dir_abs) -> Result<void>
  {
    return utils::load_spa_bundle_file_scheme(w, index_html, bundle_dir_abs);
  }
} // namespace LaVista::_internal
