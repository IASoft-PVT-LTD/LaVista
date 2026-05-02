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

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

namespace LaVista
{
  static void binding_thunk(const char *id, const char *req, void *arg)
  {
    auto *ctx = static_cast<BindingContext_T *>(arg);
    if (ctx == nullptr || ctx->window == nullptr)
    {
      return;
    }

    auto &callbacks = ctx->window->callbacks;
    const auto it = callbacks.find(ctx->event);
    if (it)
      (*it)(String(req == nullptr ? "" : req));
    if (ctx->window->webview != nullptr && id != nullptr)
      webview_return(ctx->window->webview, id, 0, "null");
  }

  static auto drag_strip_region_valid(const WindowDragStripOptions &o) -> bool
  {
    return o.end_x_percentage > o.start_x_percentage && o.end_y_percentage > o.start_y_percentage;
  }

  static auto format_drag_strip_eval_assign(const WindowDragStripOptions &o) -> std::string
  {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "if(!window.__lavistaDragStrip)window.__lavistaDragStrip={};"
                  "window.__lavistaDragStrip.sx=%.9g;"
                  "window.__lavistaDragStrip.sy=%.9g;"
                  "window.__lavistaDragStrip.ex=%.9g;"
                  "window.__lavistaDragStrip.ey=%.9g;",
                  static_cast<double>(o.start_x_percentage), static_cast<double>(o.start_y_percentage),
                  static_cast<double>(o.end_x_percentage), static_cast<double>(o.end_y_percentage));
    return std::string(buf);
  }

  static auto configure_movable_drag_strip(Window window, const WindowDragStripOptions &opts) -> Result<void>
  {
    if (window == nullptr || window->webview == nullptr)
    {
      return au::fail("Window is null");
    }

    auto drag_bind_result =
        detail::webview_error_to_result(webview_bind(
                                            window->webview, "startWindowDrag",
                                            +[](const char *id, const char *req, void *arg) {
                                              (void) req;
                                              auto *window_state = static_cast<Window>(arg);
                                              if (window_state == nullptr)
                                              {
                                                return;
                                              }

                                              detail::platform_start_window_drag(window_state);

                                              if (window_state->webview != nullptr && id != nullptr)
                                              {
                                                webview_return(window_state->webview, id, 0, "null");
                                              }
                                            },
                                            window),
                                        "webview_bind(startWindowDrag)");
    if (drag_bind_result.is_err())
    {
      return au::fail(std::move(drag_bind_result.unwrap_err()));
    }

    char init_buf[1024];
    std::snprintf(init_buf, sizeof(init_buf),
                  "(function(){"
                  "window.__lavistaDragStrip={sx:%.9g,sy:%.9g,ex:%.9g,ey:%.9g};"
                  "if(window.__lavistaDragMouseDownInstalled)return;"
                  "window.__lavistaDragMouseDownInstalled=true;"
                  "document.addEventListener('mousedown',function(e){"
                  "var o=window.__lavistaDragStrip;if(!o)return;"
                  "var w=window.innerWidth,h=window.innerHeight;"
                  "var left=w*o.sx/100.0,top=h*o.sy/100.0,right=w*o.ex/100.0,bottom=h*o.ey/100.0;"
                  "if(e.button===0&&e.clientX>=left&&e.clientX<right&&e.clientY>=top&&e.clientY<bottom){"
                  "if(window.startWindowDrag)window.startWindowDrag();}"
                  "},true);"
                  "})();",
                  static_cast<double>(opts.start_x_percentage), static_cast<double>(opts.start_y_percentage),
                  static_cast<double>(opts.end_x_percentage), static_cast<double>(opts.end_y_percentage));

    auto init_result =
        detail::webview_error_to_result(webview_init(window->webview, init_buf), "webview_init(drag-strip)");
    if (init_result.is_err())
    {
      return au::fail(std::move(init_result.unwrap_err()));
    }

    window->drag_strip_js_installed = true;
    return {};
  }

  static auto update_drag_strip_eval(Window window, const WindowDragStripOptions &opts) -> Result<void>
  {
    const std::string js = format_drag_strip_eval_assign(opts);
    return detail::webview_error_to_result(webview_eval(window->webview, js.c_str()), "webview_eval(drag-strip)");
  }

  static auto apply_window_drag_strip(Window window, const WindowDragStripOptions &opts) -> Result<void>
  {
    if (window == nullptr || window->webview == nullptr)
    {
      return au::fail("Window is null");
    }

    window->drag_strip = opts;

    if (!drag_strip_region_valid(opts))
    {
      if (window->drag_strip_js_installed)
      {
        return detail::webview_error_to_result(
            webview_eval(window->webview, "if(window.__lavistaDragStrip){"
                                          "window.__lavistaDragStrip.sx=0;window.__lavistaDragStrip.sy=0;"
                                          "window.__lavistaDragStrip.ex=0;window.__lavistaDragStrip.ey=0;}"),
            "webview_eval(drag-strip-disable)");
      }
      return {};
    }

    if (!window->drag_strip_js_installed)
    {
      return configure_movable_drag_strip(window, opts);
    }

    return update_drag_strip_eval(window, opts);
  }

  auto create_window(const WindowCreateOptions &options, const WindowDragStripOptions &drag_strip_options)
      -> Result<Window>
  {
    const i32 width = options.width;
    const i32 height = options.height;
    const i32 x = options.x;
    const i32 y = options.y;
    const String &title = options.title;
    const String &spa_bundle_path = options.spa_bundle_path;

    if (width <= 0 || height <= 0)
    {
      return au::fail("Window size must be positive");
    }

    i32 window_x = x;
    i32 window_y = y;
    {
      auto disp_res = get_displays();
      if (disp_res.is_ok())
      {
        Vec<DisplayInfo> displays = std::move(disp_res).unwrap();
        if (displays.empty())
        {
          au::panic("No displays detected");
        }
        i32 eff_display = options.display_index;
        if (eff_display < 0 || eff_display >= static_cast<i32>(displays.size()))
        {
          eff_display = 0;
        }
        const DisplayInfo &target_display = displays[static_cast<usize>(eff_display)];
        if (window_x < 0 || window_y < 0)
        {
          window_x = target_display.x + (target_display.width - width) / 2;
          window_y = target_display.y + (target_display.height - height) / 2;
        }
      }
    }

    auto state = std::make_unique<Window_T>();
    state->title = title;
    state->width = width;
    state->height = height;
    state->x = window_x;
    state->y = window_y;

    {
      auto native_result =
          detail::platform_create_window(*state, width, height, window_x, window_y, title);
      if (native_result.is_err())
      {
        return au::fail(std::move(native_result.unwrap_err()));
      }
    }

    auto set_title_result =
        detail::webview_error_to_result(webview_set_title(state->webview, title.c_str()), "webview_set_title");
    if (set_title_result.is_err())
    {
      return au::fail(std::move(set_title_result.unwrap_err()));
    }

    {
      auto post_result = detail::platform_apply_post_webview_setup(*state, width, height);
      if (post_result.is_err())
      {
        Window w = state.release();
        (void) destroy_window(w);
        return au::fail(std::move(post_result.unwrap_err()));
      }
    }

    {
      auto drag_result = apply_window_drag_strip(state.get(), drag_strip_options);
      if (drag_result.is_err())
      {
        Window w = state.release();
        (void) destroy_window(w);
        return au::fail(std::move(drag_result.unwrap_err()));
      }
    }

    Window window = state.release();

    std::filesystem::path bundle_path(spa_bundle_path.c_str());
    std::filesystem::path index_html = bundle_path;
    std::filesystem::path bundle_dir;
    if (std::filesystem::is_directory(bundle_path))
    {
      index_html /= "index.html";
      bundle_dir = std::filesystem::absolute(bundle_path);
    }
    else
    {
      bundle_dir = std::filesystem::absolute(bundle_path.parent_path());
    }

    if (!std::filesystem::exists(index_html))
    {
      (void) destroy_window(window);
      return au::fail("SPA entry file does not exist: %s", index_html.string().c_str());
    }

    const std::filesystem::path bundle_dir_abs = std::filesystem::absolute(bundle_dir);

    auto spa_result = detail::load_spa_bundle_into_webview(window->webview, index_html, bundle_dir_abs);
    if (spa_result.is_err())
    {
      (void) destroy_window(window);
      return au::fail(std::move(spa_result.unwrap_err()));
    }

    return window;
  }

  auto destroy_window(Window window) -> Result<void>
  {
    if (window == nullptr)
    {
      return au::fail("Window is null");
    }

    for (const auto &binding : window->binding_contexts)
    {
      webview_unbind(window->webview, binding.first.c_str());
    }
    window->binding_contexts.clear();
    window->callbacks.clear();

    auto destroy_result = detail::webview_error_to_result(webview_destroy(window->webview), "webview_destroy");
    window->webview = nullptr;
    window->running = false;

    detail::platform_destroy_native(window);

    delete window;
    return destroy_result;
  }

  auto update_window(Window window) -> bool
  {
    if (window == nullptr || !window->running || window->webview == nullptr)
    {
      return false;
    }

    if (!detail::platform_pump_events(window))
    {
      return false;
    }

    detail::platform_sync_window_frame_from_native(window);

    return window->running;
  }

  auto get_window_size(Window window) -> Result<Pair<i32, i32>>
  {
    if (window == nullptr)
    {
      return au::fail("Window is null");
    }

    detail::platform_sync_window_frame_from_native(window);
    return Pair<i32, i32>{window->width, window->height};
  }

  auto get_window_position(Window window) -> Result<Pair<i32, i32>>
  {
    if (window == nullptr)
    {
      return au::fail("Window is null");
    }

    detail::platform_sync_window_frame_from_native(window);
    return Pair<i32, i32>(window->x, window->y);
  }

  auto get_window_title(Window window) -> Result<String>
  {
    if (window == nullptr)
    {
      return au::fail("Window is null");
    }
    return window->title;
  }

  auto set_window_title(Window window, const String &title) -> Result<void>
  {
    if (window == nullptr)
    {
      return au::fail("Window is null");
    }
    auto set_title_result =
        detail::webview_error_to_result(webview_set_title(window->webview, title.c_str()), "webview_set_title");
    if (set_title_result.is_err())
    {
      return au::fail(std::move(set_title_result.unwrap_err()));
    }
    window->title = title;
    return {};
  }

  auto set_window_size(Window window, i32 width, i32 height) -> Result<void>
  {
    if (window == nullptr)
    {
      return au::fail("Window is null");
    }
    if (width <= 0 || height <= 0)
    {
      return au::fail("Window size must be positive");
    }

    auto r = detail::platform_set_window_size(window, width, height);
    if (r.is_err())
    {
      return au::fail(std::move(r.unwrap_err()));
    }
    window->width = width;
    window->height = height;
    return {};
  }

  auto set_window_position(Window window, i32 x, i32 y) -> Result<void>
  {
    if (window == nullptr)
    {
      return au::fail("Window is null");
    }

    auto r = detail::platform_set_window_position(window, x, y);
    if (r.is_err())
    {
      return au::fail(std::move(r.unwrap_err()));
    }
    window->x = x;
    window->y = y;
    return {};
  }

  auto set_window_drag_strip(Window window, const WindowDragStripOptions &drag_strip_options) -> Result<void>
  {
    return apply_window_drag_strip(window, drag_strip_options);
  }

  auto bind_window_event(Window window, const String &event, const std::function<void(const String &data)> &callback)
      -> Result<void>
  {
    if (window == nullptr)
    {
      return au::fail("Window is null");
    }
    if (event.empty())
    {
      return au::fail("Event name cannot be empty");
    }

    const String event_key(event.c_str());
    window->callbacks[event_key] = callback;

    auto binding_ctx = std::make_unique<BindingContext_T>();
    binding_ctx->window = window;
    binding_ctx->event = event_key;

    auto bind_result = detail::webview_error_to_result(
        webview_bind(window->webview, event.c_str(), binding_thunk, binding_ctx.get()), "webview_bind");
    if (bind_result.is_err())
    {
      return au::fail(std::move(bind_result.unwrap_err()));
    }

    window->binding_contexts.insert(event_key, std::move(binding_ctx));
    return {};
  }

  auto unbind_window_event(Window window, const String &event) -> Result<void>
  {
    if (window == nullptr)
    {
      return au::fail("Window is null");
    }
    if (event.empty())
    {
      return au::fail("Event name cannot be empty");
    }

    auto unbind_result =
        detail::webview_error_to_result(webview_unbind(window->webview, event.c_str()), "webview_unbind");
    if (unbind_result.is_err())
    {
      return au::fail(std::move(unbind_result.unwrap_err()));
    }
    window->callbacks.erase(event);
    window->binding_contexts.erase(event);
    return {};
  }

} // namespace LaVista
