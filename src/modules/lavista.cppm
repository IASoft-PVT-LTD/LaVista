// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

/**
 * @file lavista.cppm
 * @brief LaVista public module interface.
 *
 * Import with `import lavista;` to access namespace `LaVista` and re-exported LibAuxid types from `au`.
 * Use `<auxid/macros.hpp>` for `AU_TRY_*` helpers.
 */

module;

#include <functional>

export module lavista;

export import auxid;
export import lavista.definitions;

export namespace LaVista
{
  using namespace au;

  /**
   * @brief Opens a native file picker for a single file.
   *
   * @param filters NFD filter pairs (description, extension pattern, …); may be empty.
   * @param default_path Initial directory or file path shown in the dialog.
   * @return Selected path on success, empty `Option` if the user cancelled, or an error (see NFD message in result).
   */
  auto open_file_dialog(const Span<const char *const> &filters, const String &default_path = "")
      -> Result<Option<String>>;

  /**
   * @brief Opens a native file picker allowing multiple files.
   *
   * @param filters NFD filter pairs (description, extension pattern, …); may be empty.
   * @param default_path Initial directory shown in the dialog.
   * @return Selected paths on success, empty `Option` if cancelled, or an error.
   */
  auto open_files_dialog(const Span<const char *const> &filters, const String &default_path = "")
      -> Result<Option<Vec<String>>>;

  /**
   * @brief Opens a native folder picker.
   *
   * @param default_path Initial directory shown in the dialog.
   * @return Selected folder path on success, empty `Option` if cancelled, or an error.
   */
  auto open_folder_dialog(const String &default_path = "") -> Result<Option<String>>;

  /**
   * @brief Opens a native save-file dialog.
   *
   * @param filters NFD filter pairs (description, extension pattern, …); may be empty.
   * @param default_path Initial directory or suggested file path.
   * @return Chosen save path on success, empty `Option` if cancelled, or an error.
   */
  auto save_file_dialog(const Span<const char *const> &filters, const String &default_path = "")
      -> Result<Option<String>>;

  /**
   * @brief Queries connected displays.
   *
   * @return A vector of `DisplayInfo` entries with index, size, and position.
   */
  auto get_displays() -> Result<Vec<DisplayInfo>>;

  /**
   * @brief Escapes text as a JSON string literal.
   *
   * Returns `raw` wrapped in double quotes with standard JSON escaping, suitable for embedding in JSON payloads
   * or JavaScript source text.
   *
   * @param raw UTF-8 input text.
   * @return JSON string literal.
   */
  auto json_escape_for_string_literal(StringView raw) -> String;

  /**
   * @brief Serializes strings as a JSON array.
   *
   * Each element is escaped via `json_escape_for_string_literal`, e.g. `["a","b"]`.
   *
   * @param items Elements to encode.
   * @return JSON array string.
   */
  auto json_encode_array(const Vec<String> &items) -> String;

  /**
   * @brief Serializes integers as a JSON array of numbers.
   *
   * @param items Elements to encode.
   * @return JSON array string.
   */
  auto json_encode_array(const Vec<i32> &items) -> String;

  /**
   * @brief Serializes floats as a JSON array of numbers.
   *
   * Non-finite values encode as `null`.
   *
   * @param items Elements to encode.
   * @return JSON array string.
   */
  auto json_encode_array(const Vec<f32> &items) -> String;

  /**
   * @brief Creates a native window, loads the SPA bundle, and installs the default host title bar.
   *
   * The default title bar matches the hello-lavista example: `WindowCreateOptions.title` as the label and
   * `icon_path` embedded as a base64 data URI. Call `set_window_titlebar` later to replace or clear it.
   *
   * @param options Window creation options.
   * @return Opaque `Window` handle on success.
   */
  auto create_window(const WindowCreateOptions &options) -> Result<Window>;

  /**
   * @brief Destroys a window and releases native resources.
   *
   * @param window Window handle from `create_window`.
   * @return Success or an error.
   */
  auto destroy_window(Window window) -> Result<void>;

  /**
   * @brief Pumps the platform event loop for one frame.
   *
   * @param window Window handle.
   * @return `true` while the window should keep running; `false` when closed.
   */
  auto update_window(Window window) -> bool;

  /**
   * @brief Returns the current client size in pixels.
   *
   * @param window Window handle.
   * @return `(width, height)` pair.
   */
  auto get_window_size(Window window) -> Result<Pair<i32, i32>>;

  /**
   * @brief Returns the window position in virtual desktop coordinates.
   *
   * @param window Window handle.
   * @return `(x, y)` pair.
   */
  auto get_window_position(Window window) -> Result<Pair<i32, i32>>;

  /**
   * @brief Returns the current window title string.
   *
   * @param window Window handle.
   * @return Title text.
   */
  auto get_window_title(Window window) -> Result<String>;

  /**
   * @brief Sets the window title (OS chrome and default title bar label).
   *
   * @param window Window handle.
   * @param title New title text.
   * @return Success or an error.
   */
  auto set_window_title(Window window, const String &title) -> Result<void>;

  /**
   * @brief Resizes the window client area.
   *
   * @param window Window handle.
   * @param width New width in pixels.
   * @param height New height in pixels.
   * @return Success or an error.
   */
  auto set_window_size(Window window, i32 width, i32 height) -> Result<void>;

  /**
   * @brief Moves the window to the given virtual desktop coordinates.
   *
   * @param window Window handle.
   * @param x New X position.
   * @param y New Y position.
   * @return Success or an error.
   */
  auto set_window_position(Window window, i32 x, i32 y) -> Result<void>;

  /**
   * @brief Configures a drag-to-move strip over the SPA content webview.
   *
   * @param window Window handle.
   * @param drag_strip_options Region defined by percentages or pixels (see `WindowDragStripOptions`).
   * @return Success or an error.
   */
  auto set_window_drag_strip(Window window, const WindowDragStripOptions &drag_strip_options) -> Result<void>;

  /**
   * @brief Sets or clears a host-managed HTML title bar above the SPA.
   *
   * Renders `html` in a dedicated title-bar webview (replacing the default from `create_window`).
   * Pass a full HTML document or a body fragment (wrapped automatically). Pass an empty string to remove the bar;
   * window chrome bindings move back to the content webview.
   *
   * Drag-to-move for the title-bar band is enabled only when `html` is non-empty and `height_px` > 0.
   * If `height_px` is 0 with non-empty `html`, space is reserved but dragging is off.
   *
   * @param window Window handle.
   * @param html Title bar HTML, or empty to remove.
   * @param height_px Height of the title bar band in pixels.
   * @return Success or an error.
   */
  auto set_window_titlebar(Window window, const String &html, i32 height_px = 40) -> Result<void>;

  /**
   * @brief Binds a string-keyed callback invoked from the SPA via host events.
   *
   * @param window Window handle.
   * @param event Event name.
   * @param callback Handler receiving the event payload string.
   * @return Success or an error.
   */
  auto bind_window_event(Window window, const String &event, const std::function<void(const String &)> &callback)
      -> Result<void>;

  /**
   * @brief Removes a window event binding.
   *
   * @param window Window handle.
   * @param event Event name previously passed to `bind_window_event`.
   * @return Success or an error.
   */
  auto unbind_window_event(Window window, const String &event) -> Result<void>;

  /**
   * @brief Binds a host function on the SPA content webview (`window.<name>(...)`).
   *
   * The handler receives the JSON-encoded request argument from JavaScript and must return a UTF-8 string that is
   * a complete JSON value passed to the Promise on the JS side. Do not reuse the same `name` as `bind_window_event`.
   *
   * @param window Window handle.
   * @param name Function name exposed to JavaScript.
   * @param handler Callable returning a JSON value string.
   * @return Success or an error.
   */
  auto bind_window_function(Window window, const String &name, const std::function<String(const String &)> &handler)
      -> Result<void>;

  /**
   * @brief Removes a window function binding.
   *
   * @param window Window handle.
   * @param name Function name previously passed to `bind_window_function`.
   * @return Success or an error.
   */
  auto unbind_window_function(Window window, const String &name) -> Result<void>;

  /**
   * @brief Binds the default title bar menu button callback.
   *
   * @param window Window handle.
   * @param callback Invoked when the built-in title bar menu button is clicked.
   * @return Success or an error.
   */
  auto bind_window_menu_button(Window window, const std::function<void()> &callback) -> Result<void>;

  /**
   * @brief Removes the default title bar menu button callback.
   *
   * @param window Window handle.
   * @return Success or an error.
   */
  auto unbind_window_menu_button(Window window) -> Result<void>;

  /**
   * @brief Dispatches a JavaScript `CustomEvent` on the SPA content webview.
   *
   * @param window Window handle.
   * @param event_name Event type name.
   * @param detail_json Complete JSON value for `event.detail` (default `"null"`).
   * @return Success or an error.
   */
  auto dispatch_window_event(Window window, const String &event_name, const String &detail_json = "null")
      -> Result<void>;

  /**
   * @brief Dispatches a `CustomEvent` with a plain UTF-8 string payload.
   *
   * The text is JSON-escaped automatically and becomes `event.detail` on the JavaScript side.
   *
   * @param window Window handle.
   * @param event_name Event type name.
   * @param detail_text UTF-8 payload text.
   * @return Success or an error.
   */
  auto dispatch_window_event_text(Window window, const String &event_name, const String &detail_text) -> Result<void>;

  /**
   * @brief Posts binary data to the SPA script environment.
   *
   * On Windows, uses WebView2's shared buffer. On Linux, uses a custom URI scheme under the hood.
   * The SPA should use the `liblavista` package to receive buffers via `onBinaryData`.
   *
   * @param window Window handle.
   * @param buffer Raw bytes to post.
   * @return Success or an error.
   */
  auto post_binary_data(Window window, const Span<const u8> &buffer) -> Result<void>;
} // namespace LaVista
