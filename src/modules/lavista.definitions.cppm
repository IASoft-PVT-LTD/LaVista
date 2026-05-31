// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

/**
 * @file lavista.definitions.cppm
 * @brief Shared LaVista types and option structs.
 *
 * Re-exported by `import lavista`. Types live in namespace `LaVista` and use LibAuxid aliases from `au`.
 */

export module lavista.definitions;

export import auxid;

export namespace LaVista
{
  using namespace au;

  /** @brief Opaque native window handle. */
  using Window = void *;

  /** @brief Metadata for a connected display. */
  struct DisplayInfo
  {
    i32 index{-1};  ///< Zero-based display index.
    i32 width{-1};  ///< Pixel width of the display.
    i32 height{-1}; ///< Pixel height of the display.
    i32 x{-1};      ///< X coordinate of the display origin (virtual desktop space).
    i32 y{-1};      ///< Y coordinate of the display origin (virtual desktop space).
  };

  /** @brief Options passed to `create_window`. */
  struct WindowCreateOptions
  {
    String title{"LaVista App"};   ///< Initial window title and default title-bar label.
    String spa_bundle_path{""};    ///< Path to the SPA build directory (e.g. Astro/Vite `dist/`).
    String icon_path{""};          ///< Path to a PNG/JPEG icon (required on supported platforms).

    i32 width{800};   ///< Initial client width in pixels.
    i32 height{600};  ///< Initial client height in pixels.
    i32 x{-1};        ///< Initial X position; `-1` lets the platform choose.
    i32 y{-1};        ///< Initial Y position; `-1` lets the platform choose.

    i32 display_index{-1}; ///< Target display index from `get_displays`; `-1` uses the primary display.
  };

  /**
   * @brief Drag-to-move region over the SPA content webview.
   *
   * Specify either percentage bounds (`start_*_percentage` / `end_*_percentage`) or pixel bounds
   * (`start_*_px` / `end_*_px`). Pixel values take precedence when non-negative.
   */
  struct WindowDragStripOptions
  {
    f32 start_x_percentage{0.0f};   ///< Left edge of the drag strip as a percentage of client width.
    f32 start_y_percentage{0.0f};   ///< Top edge as a percentage of client height.
    f32 end_x_percentage{100.0f};   ///< Right edge as a percentage of client width.
    f32 end_y_percentage{5.0f};     ///< Bottom edge as a percentage of client height.

    i32 start_x_px{-1}; ///< Left edge in pixels; used when >= 0.
    i32 start_y_px{-1}; ///< Top edge in pixels; used when >= 0.
    i32 end_x_px{-1};   ///< Right edge in pixels; used when >= 0.
    i32 end_y_px{-1};   ///< Bottom edge in pixels; used when >= 0.
  };
} // namespace LaVista
