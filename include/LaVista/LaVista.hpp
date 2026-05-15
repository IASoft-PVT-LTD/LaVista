// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

#pragma once

#include <auxid/auxid.hpp>
#include <auxid/containers/option.hpp>
#include <auxid/containers/vec.hpp>
#include <auxid/containers/pair.hpp>

namespace LaVista
{
  using namespace au;

  typedef struct Window_T *Window;

  struct DisplayInfo
  {
    i32 index{-1};
    i32 width{-1};
    i32 height{-1};
    i32 x{-1};
    i32 y{-1};
  };

  struct WindowCreateOptions
  {
    String title{"LaVista App"};
    String spa_bundle_path{""};
    String icon_path{""};

    i32 width{800};
    i32 height{600};
    i32 x{-1};
    i32 y{-1};

    i32 display_index{-1};
  };

  struct WindowDragStripOptions
  {
    f32 start_x_percentage{0.0f};
    f32 start_y_percentage{0.0f};
    f32 end_x_percentage{100.0f};
    f32 end_y_percentage{5.0f};

    i32 start_x_px{-1};
    i32 start_y_px{-1};
    i32 end_x_px{-1};
    i32 end_y_px{-1};
  };

  /**
   * Native file dialogs (via Native File Dialog). On success, returns a path; if the user cancels, returns an empty
   * `Option`. On failure, returns an error (see `NFD_GetError` message in the result string where applicable).
   */
  auto open_file_dialog(const Span<const char *const> &filters, const String &default_path = "")
      -> Result<Option<String>>;
  auto open_files_dialog(const Span<const char *const> &filters, const String &default_path = "")
      -> Result<Option<Vec<String>>>;
  auto open_folder_dialog(const String &default_path = "") -> Result<Option<String>>;
  auto save_file_dialog(const Span<const char *const> &filters, const String &default_path = "")
      -> Result<Option<String>>;

  auto get_displays() -> Result<Vec<DisplayInfo>>;

  /**
   * Escapes `raw` as a JSON string literal (including surrounding double quotes), suitable for concatenating into JSON
   * payloads or JavaScript source text.
   */
  auto json_escape_for_string_literal(StringView raw) -> String;

  /**
   * Serializes `items` as a JSON array.
   * - `Vec<String>`: JSON strings, `["a","b"]`, via `json_escape_for_string_literal` per element.
   * - `Vec<i32>` / `Vec<f32>`: JSON numbers; non-finite floats encode as `null`.
   */
  auto json_encode_array(const Vec<String> &items) -> String;
  auto json_encode_array(const Vec<i32> &items) -> String;
  auto json_encode_array(const Vec<f32> &items) -> String;

  /**
   * Creates the native window, loads the SPA bundle, and applies LaVista's default host title bar: same chrome as the
   * hello-lavista example, with `WindowCreateOptions.title` as the label and `icon_path` embedded (base64 data URI) for
   * the image next to the title. Call `set_window_titlebar` later to replace or clear it.
   */
  auto create_window(const WindowCreateOptions &options) -> Result<Window>;
  auto destroy_window(Window window) -> Result<void>;
  auto update_window(Window window) -> bool;

  auto get_window_size(Window window) -> Result<Pair<i32, i32>>;
  auto get_window_position(Window window) -> Result<Pair<i32, i32>>;
  auto get_window_title(Window window) -> Result<String>;

  auto set_window_title(Window window, const String &title) -> Result<void>;
  auto set_window_size(Window window, i32 width, i32 height) -> Result<void>;
  auto set_window_position(Window window, i32 x, i32 y) -> Result<void>;

  auto set_window_drag_strip(Window window, const WindowDragStripOptions &drag_strip_options) -> Result<void>;

  /**
   * Host-managed title bar rendered above the SPA in a separate webview (replacing the default title bar applied by
   * `create_window`). Pass HTML for a full document, or a body fragment (wrapped automatically). Pass an empty string
   * to remove the title bar; window chrome bindings move back to the content webview.
   *
   * Drag-to-move for the title-bar band is enabled only when `html` is non-empty and `height_px` > 0 (strip height is
   * `height_px` within the band). If `height_px` is 0 with non-empty `html`, the band still reserves space but dragging
   * is off.
   */
  auto set_window_titlebar(Window window, const String &html, i32 height_px = 40) -> Result<void>;

  auto bind_window_event(Window window, const String &event, const Function<void, const String &> &callback)
      -> Result<void>;
  auto unbind_window_event(Window window, const String &event) -> Result<void>;

  /**
   * Binds a host function on the SPA content webview only (`window.<name>(...)`). The handler receives the JSON-encoded
   * request argument from JavaScript (often a serialized object or empty string) and must return a UTF-8 string that is
   * a complete JSON value (`null`, booleans, numbers, strings, arrays, objects) passed to the Promise on the JS side.
   * Underlying webview/GTK APIs are not exposed. Do not use the same `name` as `bind_window_event`.
   */
  auto bind_window_function(Window window, const String &name, const Function<String, const String &> &handler)
      -> Result<void>;
  auto unbind_window_function(Window window, const String &name) -> Result<void>;

  /**
   * Binds the default titlebar menu button callback.
   * The callback runs when the menu button in LaVista's built-in titlebar is clicked.
   */
  auto bind_window_menu_button(Window window, const Function<void> &callback) -> Result<void>;
  auto unbind_window_menu_button(Window window) -> Result<void>;

  /**
   * Dispatches a JavaScript `CustomEvent` on the SPA content webview (`window.dispatchEvent(...)`).
   * `detail_json` must be a complete JSON value (object, array, string, number, bool, or null).
   */
  auto dispatch_window_event(Window window, const String &event_name, const String &detail_json = "null")
      -> Result<void>;

  /**
   * Convenience helper that dispatches a JavaScript `CustomEvent` with a plain UTF-8 string payload.
   * The payload is JSON-escaped automatically and becomes `event.detail` on the JavaScript side.
   */
  auto dispatch_window_event_text(Window window, const String &event_name, const String &detail_text) -> Result<void>;
  /**
   * Posts binary data to the SPA script environment.
   * On Windows, this uses WebView2's shared buffer.
   * On Linux, this uses a custom URI scheme under the hood.
   */
  auto post_binary_data(Window window, const Span<const u8> &buffer) -> Result<void>;
} // namespace LaVista
