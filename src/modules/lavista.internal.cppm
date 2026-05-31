// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

module;

#include <functional>

#define WEBVIEW_HEADER
#include <webview/webview.h>
#undef WEBVIEW_HEADER

#include <LaVista_platform_context.hpp>

export module lavista.internal;

import auxid;
import lavista.definitions;

/** @cond INTERNAL */
export namespace LaVista
{
  using namespace au;

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
    HashMap<String, std::function<void(const String &)>> callbacks;
    std::function<void()> menu_button_callback;
    bool menu_button_callback_bound = false;
    HashMap<String, Box<BindingContext_T>> binding_contexts;
    HashMap<String, std::function<String(const String &)>> json_binding_handlers;
    HashMap<String, Box<JsonBindingContext_T>> json_binding_contexts;
    HashMap<String, Vec<u8>> pending_binary_buffers;
    u64 next_binary_buffer_id = 0;
    Box<_internal::DragBindCtx> content_drag_bind_ctx{nullptr};
    Box<_internal::DragBindCtx> titlebar_drag_bind_ctx{nullptr};
    Box<_internal::ChromeBindCtx> chrome_bind_ctx{nullptr};
    PlatformWindowContext platform{};
    Vec<filesystem::Path> temp_files;
  };

  [[nodiscard]] inline auto window_ptr(Window handle) noexcept -> Window_T *
  {
    return static_cast<Window_T *>(handle);
  }

  namespace _internal
  {
    inline constexpr wchar_t SPA_VIRTUAL_HOST_NAME_W[] = L"lavista.bundle.invalid";
    inline constexpr char SPA_VIRTUAL_HOST_NAME[] = "lavista.bundle.invalid";

    auto ensure_gtk_initialized() -> Result<void>;

    auto apply_webview2_default_background(webview_t w) -> void;
    auto map_webview2_spa_virtual_host(webview_t w, const filesystem::Path &bundle_dir_abs) -> Result<void>;

    inline auto webview_error_to_result(webview_error_t err, const char *context) -> Result<void>
    {
      if (WEBVIEW_SUCCEEDED(err))
      {
        return {};
      }
      return fail("{} failed with webview error code {}", context, static_cast<int>(err));
    }

    auto platform_get_displays() -> Result<Vec<DisplayInfo>>;

    auto platform_create_window(Window_T &state, i32 width, i32 height, i32 window_x, i32 window_y, const String &title,
                                const String &icon_path) -> Result<void>;

    auto platform_apply_post_webview_setup(Window_T &state, i32 width, i32 height) -> Result<void>;

    auto platform_destroy_native(Window window) -> void;

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

    auto platform_post_binary_data(Window window, const Span<const u8> &buffer) -> Result<String>;
  } // namespace _internal

  namespace utils
  {
    auto load_image_rgba_from_file(const String &path, Vec<u8> &out_rgba, i32 &out_w, i32 &out_h) -> Result<void>;

    auto resize_rgba_nearest(const u8 *src, i32 sw, i32 sh, i32 dw, i32 dh, Vec<u8> &out) -> void;

    auto load_spa_bundle_file_scheme(Window window, webview_t w, const filesystem::Path &index_html,
                                     const filesystem::Path &bundle_dir_abs) -> Result<void>;
  } // namespace utils
} // namespace LaVista
/** @endcond */
