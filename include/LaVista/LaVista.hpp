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
