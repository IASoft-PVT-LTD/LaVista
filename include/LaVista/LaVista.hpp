#pragma once

#include <auxid/auxid.hpp>
#include <auxid/containers/vec.hpp>
#include <auxid/containers/pair.hpp>

#include <functional>

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
  };

  auto get_displays() -> Result<Vec<DisplayInfo>>;

  auto create_window(const WindowCreateOptions &options, const WindowDragStripOptions &drag_strip_options = {})
      -> Result<Window>;
  auto destroy_window(Window window) -> Result<void>;
  auto update_window(Window window) -> bool;

  auto get_window_size(Window window) -> Result<Pair<i32, i32>>;
  auto get_window_position(Window window) -> Result<Pair<i32, i32>>;
  auto get_window_title(Window window) -> Result<String>;

  auto set_window_title(Window window, const String &title) -> Result<void>;
  auto set_window_size(Window window, i32 width, i32 height) -> Result<void>;
  auto set_window_position(Window window, i32 x, i32 y) -> Result<void>;

  auto set_window_drag_strip(Window window, const WindowDragStripOptions &drag_strip_options) -> Result<void>;

  auto bind_window_event(Window window, const String &event, const std::function<void(const String &data)> &callback)
      -> Result<void>;
  auto unbind_window_event(Window window, const String &event) -> Result<void>;
} // namespace LaVista
