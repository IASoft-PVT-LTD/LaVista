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

#include <cctype>
#include <cstdio>
#include <cstring>

namespace LaVista
{
  namespace
  {
    auto tag_name_case_insensitive(StringView s, usize i, const char *tag_lower) -> bool
    {
      for (usize k = 0; tag_lower[k] != '\0'; ++k)
      {
        if (i + k >= s.size())
        {
          return false;
        }
        const char c = s[i + k];
        const char e = tag_lower[k];
        const char cl = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        if (cl != e)
        {
          return false;
        }
      }
      return true;
    }

    auto looks_like_full_html_document(StringView s) -> bool
    {
      usize i = 0;
      while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
      {
        ++i;
      }
      if (i >= s.size() || s[i] != '<')
      {
        return false;
      }
      if (tag_name_case_insensitive(s, i + 1, "html"))
      {
        return true;
      }
      if (i + 9 <= s.size() && tag_name_case_insensitive(s, i + 1, "!doctype"))
      {
        return true;
      }
      return false;
    }

    auto wrap_titlebar_html_body(const String &html) -> String
    {
      if (looks_like_full_html_document(StringView(html.c_str())))
      {
        return html.clone();
      }
      return String("<!DOCTYPE html><html><head><meta charset=\"utf-8\"/>"
                    "<style>html,body{margin:0;padding:0;width:100%;height:100%;overflow:hidden;}"
                    "</style></head><body>") +
             html + "</body></html>";
    }

    auto unbind_start_drag(webview_t w) -> void
    {
      if (w != nullptr)
      {
        (void) webview_unbind(w, "startWindowDrag");
      }
    }

    auto unbind_chrome(webview_t w) -> void
    {
      if (w == nullptr)
      {
        return;
      }
      (void) webview_unbind(w, "LaVista_Minimize");
      (void) webview_unbind(w, "LaVista_Menu");
      (void) webview_unbind(w, "LaVista_Maximize");
      (void) webview_unbind(w, "LaVista_QueryMaximized");
      (void) webview_unbind(w, "LaVista_Close");
    }
  } // namespace

  static auto binding_thunk(const char *id, const char *req, void *arg) -> void
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

  static auto json_binding_thunk(const char *id, const char *req, void *arg) -> void
  {
    auto *ctx = static_cast<JsonBindingContext_T *>(arg);
    if (ctx == nullptr || ctx->window == nullptr)
    {
      return;
    }

    auto *handler = ctx->window->json_binding_handlers.find(ctx->name);
    if (handler == nullptr)
    {
      if (ctx->window->webview != nullptr && id != nullptr)
      {
        webview_return(ctx->window->webview, id, 0, "null");
      }
      return;
    }

    const String out = (*handler)(String(req == nullptr ? "" : req));
    if (ctx->window->webview != nullptr && id != nullptr)
    {
      webview_return(ctx->window->webview, id, 0, out.c_str());
    }
  }

  static auto drag_strip_region_valid(const WindowDragStripOptions &o) -> bool
  {
    return o.end_x_percentage > o.start_x_percentage && o.end_y_percentage > o.start_y_percentage;
  }

  static auto format_drag_strip_eval_assign(const WindowDragStripOptions &o) -> String
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
    return String(buf);
  }

  static auto configure_movable_drag_strip(Window window, webview_t target, const WindowDragStripOptions &opts,
                                           bool *installed_flag, memory::Box<_internal::DragBindCtx> &ctx_storage)
      -> Result<void>
  {
    if (window == nullptr || target == nullptr)
    {
      return fail("Window is null");
    }

    unbind_start_drag(target);
    ctx_storage = memory::make_box<_internal::DragBindCtx>();
    ctx_storage->window = window;
    ctx_storage->sender = target;

    auto drag_bind_result = _internal::webview_error_to_result(webview_bind(
                                                                   target, "startWindowDrag",
                                                                   +[](const char *id, const char *req, void *arg) {
                                                                     (void) req;
                                                                     auto *ctx =
                                                                         static_cast<_internal::DragBindCtx *>(arg);
                                                                     if (ctx == nullptr || ctx->window == nullptr)
                                                                     {
                                                                       return;
                                                                     }
                                                                     _internal::platform_start_window_drag(ctx->window);
                                                                     if (ctx->sender != nullptr && id != nullptr)
                                                                     {
                                                                       webview_return(ctx->sender, id, 0, "null");
                                                                     }
                                                                   },
                                                                   ctx_storage.get()),
                                                               "webview_bind(startWindowDrag)");
    if (drag_bind_result.is_err())
    {
      ctx_storage.reset();
      return fail(std::move(drag_bind_result.unwrap_err()));
    }

    char init_buf[1152];
    std::snprintf(init_buf, sizeof(init_buf),
                  "(function(){"
                  "window.__lavistaDragStrip={sx:%.9g,sy:%.9g,ex:%.9g,ey:%.9g};"
                  "if(window.__lavistaDragMouseDownInstalled)return;"
                  "window.__lavistaDragMouseDownInstalled=true;"
                  "document.addEventListener('mousedown',function(e){"
                  "var o=window.__lavistaDragStrip;if(!o)return;"
                  "if(e.target.closest('button,a,[data-lavista-no-drag]'))return;"
                  "var w=window.innerWidth,h=window.innerHeight;"
                  "var left=w*o.sx/100.0,top=h*o.sy/100.0,right=w*o.ex/100.0,bottom=h*o.ey/100.0;"
                  "if(e.button===0&&e.clientX>=left&&e.clientX<right&&e.clientY>=top&&e.clientY<bottom){"
                  "if(window.startWindowDrag)window.startWindowDrag();}"
                  "},true);"
                  "})();",
                  static_cast<double>(opts.start_x_percentage), static_cast<double>(opts.start_y_percentage),
                  static_cast<double>(opts.end_x_percentage), static_cast<double>(opts.end_y_percentage));

    /* webview_init only registers scripts via AddScriptToExecuteOnDocumentCreated (WebView2) /
     * WebKitUserScript (GTK) for *future* navigations. The titlebar/content page is already loaded by the time we
     * get here, so without also evaluating the script on the current page the mousedown listener is never
     * installed — leaving `window.startWindowDrag()` reachable but never called. The script is idempotent
     * (guarded by `__lavistaDragMouseDownInstalled`), so it is safe to run via both channels. */
    auto init_result = _internal::webview_error_to_result(webview_init(target, init_buf), "webview_init(drag-strip)");
    if (init_result.is_err())
    {
      ctx_storage.reset();
      return fail(std::move(init_result.unwrap_err()));
    }

    auto eval_result = _internal::webview_error_to_result(webview_eval(target, init_buf), "webview_eval(drag-strip)");
    if (eval_result.is_err())
    {
      ctx_storage.reset();
      return fail(std::move(eval_result.unwrap_err()));
    }

    if (installed_flag != nullptr)
    {
      *installed_flag = true;
    }
    return {};
  }

  static auto update_drag_strip_eval(Window window, const WindowDragStripOptions &opts) -> Result<void>
  {
    const String js = format_drag_strip_eval_assign(opts);
    return _internal::webview_error_to_result(webview_eval(window->webview, js.c_str()), "webview_eval(drag-strip)");
  }

  /**
   * Runs after each title-bar HTML load. Always uses webview_init for valid regions so drag handlers survive
   * navigations.
   */
  static auto apply_titlebar_drag_strip_after_load(Window window, const WindowDragStripOptions &opts) -> Result<void>
  {
    if (window == nullptr || window->titlebar_webview == nullptr)
    {
      return fail("Title bar is not active");
    }

    if (!drag_strip_region_valid(opts))
    {
      if (window->titlebar_drag_strip_js_installed)
      {
        return _internal::webview_error_to_result(
            webview_eval(window->titlebar_webview, "if(window.__lavistaDragStrip){"
                                                   "window.__lavistaDragStrip.sx=0;window.__lavistaDragStrip.sy=0;"
                                                   "window.__lavistaDragStrip.ex=0;window.__lavistaDragStrip.ey=0;}"),
            "webview_eval(titlebar drag-strip-disable)");
      }
      return {};
    }

    return configure_movable_drag_strip(window, window->titlebar_webview, opts,
                                        &window->titlebar_drag_strip_js_installed, window->titlebar_drag_bind_ctx);
  }

  static auto apply_window_drag_strip(Window window, const WindowDragStripOptions &opts) -> Result<void>
  {
    if (window == nullptr || window->webview == nullptr)
    {
      return fail("Window is null");
    }

    window->drag_strip = opts;

    if (!drag_strip_region_valid(opts))
    {
      if (window->content_drag_strip_js_installed)
      {
        return _internal::webview_error_to_result(
            webview_eval(window->webview, "if(window.__lavistaDragStrip){"
                                          "window.__lavistaDragStrip.sx=0;window.__lavistaDragStrip.sy=0;"
                                          "window.__lavistaDragStrip.ex=0;window.__lavistaDragStrip.ey=0;}"),
            "webview_eval(drag-strip-disable)");
      }
      return {};
    }

    if (!window->content_drag_strip_js_installed)
    {
      return configure_movable_drag_strip(window, window->webview, opts, &window->content_drag_strip_js_installed,
                                          window->content_drag_bind_ctx);
    }

    return update_drag_strip_eval(window, opts);
  }

  static auto install_window_chrome_bindings(Window window, webview_t sender) -> Result<void>
  {
    if (window == nullptr || sender == nullptr)
    {
      return fail("Window is null");
    }

    if (window->chrome_bind_ctx != nullptr && window->chrome_bind_ctx->sender != nullptr &&
        window->chrome_bind_ctx->sender != sender)
    {
      unbind_chrome(window->chrome_bind_ctx->sender);
    }

    window->chrome_bind_ctx = memory::make_box<_internal::ChromeBindCtx>();
    window->chrome_bind_ctx->window = window;
    window->chrome_bind_ctx->sender = sender;

    _internal::ChromeBindCtx *ctx = window->chrome_bind_ctx.get();

    auto minimize_bind =
        _internal::webview_error_to_result(webview_bind(
                                               sender, "LaVista_Minimize",
                                               +[](const char *id, const char *req, void *arg) {
                                                 (void) req;
                                                 auto *c = static_cast<_internal::ChromeBindCtx *>(arg);
                                                 if (c != nullptr && c->window != nullptr)
                                                 {
                                                   _internal::platform_minimize_window(c->window);
                                                 }
                                                 if (c != nullptr && c->sender != nullptr && id != nullptr)
                                                 {
                                                   webview_return(c->sender, id, 0, "null");
                                                 }
                                               },
                                               ctx),
                                           "webview_bind(LaVista_Minimize)");
    if (minimize_bind.is_err())
    {
      return fail(std::move(minimize_bind.unwrap_err()));
    }

    auto menu_bind = _internal::webview_error_to_result(
        webview_bind(
            sender, "LaVista_Menu",
            +[](const char *id, const char *req, void *arg) {
              (void) req;
              auto *c = static_cast<_internal::ChromeBindCtx *>(arg);
              if (c != nullptr && c->window != nullptr && c->window->menu_button_callback_bound)
              {
                c->window->menu_button_callback();
              }
              if (c != nullptr && c->sender != nullptr && id != nullptr)
              {
                webview_return(c->sender, id, 0, "null");
              }
            },
            ctx),
        "webview_bind(LaVista_Menu)");
    if (menu_bind.is_err())
    {
      return fail(std::move(menu_bind.unwrap_err()));
    }

    auto maximize_bind =
        _internal::webview_error_to_result(webview_bind(
                                               sender, "LaVista_Maximize",
                                               +[](const char *id, const char *req, void *arg) {
                                                 (void) req;
                                                 auto *c = static_cast<_internal::ChromeBindCtx *>(arg);
                                                 if (c != nullptr && c->window != nullptr)
                                                 {
                                                   _internal::platform_toggle_maximize_window(c->window);
                                                 }
                                                 if (c != nullptr && c->sender != nullptr && id != nullptr)
                                                 {
                                                   webview_return(c->sender, id, 0, "null");
                                                 }
                                               },
                                               ctx),
                                           "webview_bind(LaVista_Maximize)");
    if (maximize_bind.is_err())
    {
      return fail(std::move(maximize_bind.unwrap_err()));
    }

    auto query_max_bind = _internal::webview_error_to_result(
        webview_bind(
            sender, "LaVista_QueryMaximized",
            +[](const char *id, const char *req, void *arg) {
              (void) req;
              auto *c = static_cast<_internal::ChromeBindCtx *>(arg);
              const bool maximized =
                  c != nullptr && c->window != nullptr && _internal::platform_window_is_maximized(c->window);
              if (c != nullptr && c->sender != nullptr && id != nullptr)
              {
                webview_return(c->sender, id, 0, maximized ? "true" : "false");
              }
            },
            ctx),
        "webview_bind(LaVista_QueryMaximized)");
    if (query_max_bind.is_err())
    {
      return fail(std::move(query_max_bind.unwrap_err()));
    }

    auto close_bind =
        _internal::webview_error_to_result(webview_bind(
                                               sender, "LaVista_Close",
                                               +[](const char *id, const char *req, void *arg) {
                                                 (void) req;
                                                 auto *c = static_cast<_internal::ChromeBindCtx *>(arg);
                                                 if (c != nullptr && c->window != nullptr)
                                                 {
                                                   _internal::platform_close_window(c->window);
                                                 }
                                                 if (c != nullptr && c->sender != nullptr && id != nullptr)
                                                 {
                                                   webview_return(c->sender, id, 0, "null");
                                                 }
                                               },
                                               ctx),
                                           "webview_bind(LaVista_Close)");
    if (close_bind.is_err())
    {
      return fail(std::move(close_bind.unwrap_err()));
    }

    return {};
  }

  auto set_window_titlebar(Window window, const String &html, i32 height_px) -> Result<void>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }

    window->titlebar_html = html;

    if (html.empty())
    {
      if (window->titlebar_webview != nullptr)
      {
        unbind_chrome(window->titlebar_webview);
        unbind_start_drag(window->titlebar_webview);
        window->titlebar_drag_bind_ctx.reset();
        window->titlebar_drag_strip_js_installed = false;
        auto tb = _internal::platform_destroy_titlebar_webview(window);
        if (tb.is_err())
        {
          return fail(std::move(tb.unwrap_err()));
        }
      }
      window->titlebar_height_px = 0;
      window->titlebar_html.clear();
      auto chrome_on_main = install_window_chrome_bindings(window, window->webview);
      if (chrome_on_main.is_err())
      {
        return fail(std::move(chrome_on_main.unwrap_err()));
      }
      if (window->drag_strip_backup_valid)
      {
        auto strip_restore = apply_window_drag_strip(window, window->drag_strip_backup);
        window->drag_strip_backup_valid = false;
        if (strip_restore.is_err())
        {
          return fail(std::move(strip_restore.unwrap_err()));
        }
      }
      _internal::platform_layout_webviews(window);
      return {};
    }

    const bool drag_strip_enabled = height_px > 0;
    const i32 layout_px = drag_strip_enabled ? height_px : 40;
    window->titlebar_height_px = layout_px;

    const bool created_now = window->titlebar_webview == nullptr;
    if (created_now)
    {
      auto mk = _internal::platform_create_titlebar_webview(window);
      if (mk.is_err())
      {
        return fail(std::move(mk.unwrap_err()));
      }
      auto chrome_move = install_window_chrome_bindings(window, window->titlebar_webview);
      if (chrome_move.is_err())
      {
        [[maybe_unused]] auto const destroyed = _internal::platform_destroy_titlebar_webview(window);
        (void) install_window_chrome_bindings(window, window->webview);
        return fail(std::move(chrome_move.unwrap_err()));
      }
    }

    const String doc = wrap_titlebar_html_body(html);
    auto nav = _internal::load_inline_html_into_webview(window->titlebar_webview, doc);
    if (nav.is_err())
    {
      if (created_now)
      {
        [[maybe_unused]] auto const destroyed = _internal::platform_destroy_titlebar_webview(window);
        (void) install_window_chrome_bindings(window, window->webview);
      }
      return fail(std::move(nav.unwrap_err()));
    }

    WindowDragStripOptions tb{};
    if (drag_strip_enabled)
    {
      tb.start_x_percentage = 0.f;
      tb.start_y_percentage = 0.f;
      tb.end_x_percentage = 100.f;
      const f32 layout_f = static_cast<f32>(layout_px <= 0 ? 1 : layout_px);
      const f32 ey = static_cast<f32>(height_px) * 100.f / layout_f;
      tb.end_y_percentage = ey > 100.f ? 100.f : (ey < 0.f ? 0.f : ey);
    }
    else
    {
      tb.start_x_percentage = 0.f;
      tb.end_x_percentage = 0.f;
      tb.start_y_percentage = 0.f;
      tb.end_y_percentage = 0.f;
    }

    auto tdrag = apply_titlebar_drag_strip_after_load(window, tb);
    if (tdrag.is_err())
    {
      return fail(std::move(tdrag.unwrap_err()));
    }

    _internal::platform_layout_webviews(window);

    /* First time we attach a host title bar: park SPA drag-strip settings and disable strip on the content webview so
       only the title bar moves the window (updates that only change HTML skip this). */
    if (created_now)
    {
      window->drag_strip_backup = window->drag_strip;
      window->drag_strip_backup_valid = true;
      WindowDragStripOptions off{};
      off.start_x_percentage = 0.f;
      off.end_x_percentage = 0.f;
      off.start_y_percentage = 0.f;
      off.end_y_percentage = 0.f;
      auto strip_off = apply_window_drag_strip(window, off);
      if (strip_off.is_err())
      {
        return fail(std::move(strip_off.unwrap_err()));
      }
    }

    return {};
  }

  auto create_window(const WindowCreateOptions &options) -> Result<Window>
  {
    const i32 width = options.width;
    const i32 height = options.height;
    const i32 x = options.x;
    const i32 y = options.y;
    const String &title = options.title;
    const String &spa_bundle_path = options.spa_bundle_path;

    if (width <= 0 || height <= 0)
    {
      return fail("Window size must be positive");
    }

    if (options.icon_path.empty())
    {
      return fail("WindowCreateOptions.icon_path is required (PNG, JPEG, or other format supported by stb_image)");
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
          panic("No displays detected");
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

    auto state = memory::make_box<Window_T>();
    state->title = title;
    state->width = width;
    state->height = height;
    state->x = window_x;
    state->y = window_y;

    {
      auto native_result =
          _internal::platform_create_window(*state, width, height, window_x, window_y, title, options.icon_path);
      if (native_result.is_err())
      {
        return fail(std::move(native_result.unwrap_err()));
      }
    }

    auto set_title_result =
        _internal::webview_error_to_result(webview_set_title(state->webview, title.c_str()), "webview_set_title");
    if (set_title_result.is_err())
    {
      return fail(std::move(set_title_result.unwrap_err()));
    }

    {
      auto post_result = _internal::platform_apply_post_webview_setup(*state, width, height);
      if (post_result.is_err())
      {
        Window w = state.leak();
        (void) destroy_window(w);
        return fail(std::move(post_result.unwrap_err()));
      }
    }

    Window window = state.leak();

    const filesystem::Path bundle_path(spa_bundle_path.c_str());
    filesystem::Path index_html = bundle_path;
    filesystem::Path bundle_dir;

    AU_TRY_VAR(is_dir, filesystem::is_directory(bundle_path));
    if (is_dir)
    {
      index_html /= "index.html";
      AU_TRY_VAR(abs_bundle, filesystem::absolute(bundle_path));
      bundle_dir = std::move(abs_bundle);
    }
    else
    {
      AU_TRY_VAR(abs_parent, filesystem::absolute(bundle_path.parent_path()));
      bundle_dir = std::move(abs_parent);
    }

    AU_TRY_VAR(entry_exists, filesystem::exists(index_html));
    if (!entry_exists)
    {
      (void) destroy_window(window);
      return fail("SPA entry file does not exist: %s", index_html.string().c_str());
    }

    AU_TRY_VAR(bundle_dir_abs, filesystem::absolute(bundle_dir));

    auto spa_result = _internal::load_spa_bundle_into_webview(window->webview, index_html, bundle_dir_abs);
    if (spa_result.is_err())
    {
      (void) destroy_window(window);
      return fail(std::move(spa_result.unwrap_err()));
    }

    {
      auto chrome = install_window_chrome_bindings(window, window->webview);
      if (chrome.is_err())
      {
        (void) destroy_window(window);
        return fail(std::move(chrome.unwrap_err()));
      }
    }

    {
      auto html_res = _internal::build_default_titlebar_html(title, options.icon_path);
      if (html_res.is_err())
      {
        (void) destroy_window(window);
        return fail(std::move(html_res.unwrap_err()));
      }
      String default_titlebar = std::move(html_res).unwrap();
      auto tb_res = set_window_titlebar(window, default_titlebar, 40);
      if (tb_res.is_err())
      {
        (void) destroy_window(window);
        return fail(std::move(tb_res.unwrap_err()));
      }
    }

    return window;
  }

  auto destroy_window(Window window) -> Result<void>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }

    for (const auto &binding : window->binding_contexts)
    {
      webview_unbind(window->webview, binding.first.c_str());
    }
    window->binding_contexts.clear();
    window->callbacks.clear();

    for (const auto &binding : window->json_binding_contexts)
    {
      webview_unbind(window->webview, binding.first.c_str());
    }
    window->json_binding_contexts.clear();
    window->json_binding_handlers.clear();

    if (window->chrome_bind_ctx != nullptr && window->chrome_bind_ctx->sender != nullptr)
    {
      unbind_chrome(window->chrome_bind_ctx->sender);
    }
    window->chrome_bind_ctx.reset();

    unbind_start_drag(window->webview);
    if (window->titlebar_webview != nullptr)
    {
      unbind_start_drag(window->titlebar_webview);
    }

    if (window->titlebar_webview != nullptr)
    {
      auto tb_destroy = _internal::platform_destroy_titlebar_webview(window);
      if (tb_destroy.is_err())
      {
        (void) _internal::webview_error_to_result(webview_destroy(window->webview), "webview_destroy");
        window->webview = nullptr;
        window->running = false;
        _internal::platform_destroy_native(window);
        delete window;
        return fail(std::move(tb_destroy.unwrap_err()));
      }
    }

    auto destroy_result = _internal::webview_error_to_result(webview_destroy(window->webview), "webview_destroy");
    window->webview = nullptr;
    window->running = false;

    _internal::platform_destroy_native(window);

    delete window;
    return destroy_result;
  }

  auto update_window(Window window) -> bool
  {
    if (window == nullptr || !window->running || window->webview == nullptr)
    {
      return false;
    }

    if (!_internal::platform_pump_events(window))
    {
      return false;
    }

    _internal::platform_sync_window_frame_from_native(window);

    return window->running;
  }

  auto get_window_size(Window window) -> Result<Pair<i32, i32>>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }

    _internal::platform_sync_window_frame_from_native(window);
    return Pair<i32, i32>{window->width, window->height};
  }

  auto get_window_position(Window window) -> Result<Pair<i32, i32>>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }

    _internal::platform_sync_window_frame_from_native(window);
    return Pair<i32, i32>(window->x, window->y);
  }

  auto get_window_title(Window window) -> Result<String>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }
    return window->title;
  }

  auto set_window_title(Window window, const String &title) -> Result<void>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }
    auto set_title_result =
        _internal::webview_error_to_result(webview_set_title(window->webview, title.c_str()), "webview_set_title");
    if (set_title_result.is_err())
    {
      return fail(std::move(set_title_result.unwrap_err()));
    }
    window->title = title;
    return {};
  }

  auto set_window_size(Window window, i32 width, i32 height) -> Result<void>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }
    if (width <= 0 || height <= 0)
    {
      return fail("Window size must be positive");
    }

    auto r = _internal::platform_set_window_size(window, width, height);
    if (r.is_err())
    {
      return fail(std::move(r.unwrap_err()));
    }
    window->width = width;
    window->height = height;
    return {};
  }

  auto set_window_position(Window window, i32 x, i32 y) -> Result<void>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }

    auto r = _internal::platform_set_window_position(window, x, y);
    if (r.is_err())
    {
      return fail(std::move(r.unwrap_err()));
    }
    window->x = x;
    window->y = y;
    return {};
  }

  auto set_window_drag_strip(Window window, const WindowDragStripOptions &drag_strip_options) -> Result<void>
  {
    return apply_window_drag_strip(window, drag_strip_options);
  }

  auto bind_window_event(Window window, const String &event, const Function<void, const String &> &callback)
      -> Result<void>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }
    if (event.empty())
    {
      return fail("Event name cannot be empty");
    }

    const String event_key(event.c_str());
    if (window->json_binding_handlers.find(event_key) != nullptr)
    {
      return fail("Name is already bound as a window function; unbind it first");
    }

    window->callbacks[event_key] = callback;

    auto binding_ctx = memory::make_box<BindingContext_T>();
    binding_ctx->window = window;
    binding_ctx->event = event_key;

    auto bind_result = _internal::webview_error_to_result(
        webview_bind(window->webview, event.c_str(), binding_thunk, binding_ctx.get()), "webview_bind");
    if (bind_result.is_err())
    {
      return fail(std::move(bind_result.unwrap_err()));
    }

    window->binding_contexts.insert(event_key, std::move(binding_ctx));
    return {};
  }

  auto unbind_window_event(Window window, const String &event) -> Result<void>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }
    if (event.empty())
    {
      return fail("Event name cannot be empty");
    }

    auto unbind_result =
        _internal::webview_error_to_result(webview_unbind(window->webview, event.c_str()), "webview_unbind");
    if (unbind_result.is_err())
    {
      return fail(std::move(unbind_result.unwrap_err()));
    }
    window->callbacks.erase(event);
    window->binding_contexts.erase(event);
    return {};
  }

  auto bind_window_function(Window window, const String &name, const Function<String, const String &> &handler)
      -> Result<void>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }
    if (name.empty())
    {
      return fail("Function name cannot be empty");
    }

    const String name_key(name.c_str());
    if (window->callbacks.find(name_key) != nullptr)
    {
      return fail("Name is already bound as a window event; unbind it first");
    }

    window->json_binding_handlers[name_key] = handler;

    if (window->json_binding_contexts.find(name_key) != nullptr)
    {
      return {};
    }

    auto binding_ctx = memory::make_box<JsonBindingContext_T>();
    binding_ctx->window = window;
    binding_ctx->name = name_key;

    auto bind_result =
        _internal::webview_error_to_result(webview_bind(window->webview, name.c_str(), json_binding_thunk, binding_ctx.get()),
                                           "webview_bind(window function)");
    if (bind_result.is_err())
    {
      window->json_binding_handlers.erase(name_key);
      return fail(std::move(bind_result.unwrap_err()));
    }

    window->json_binding_contexts.insert(name_key, std::move(binding_ctx));
    return {};
  }

  auto unbind_window_function(Window window, const String &name) -> Result<void>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }
    if (name.empty())
    {
      return fail("Function name cannot be empty");
    }

    const String name_key(name.c_str());
    if (window->json_binding_contexts.find(name_key) == nullptr)
    {
      return fail("Function is not bound");
    }

    auto unbind_result =
        _internal::webview_error_to_result(webview_unbind(window->webview, name.c_str()), "webview_unbind");
    if (unbind_result.is_err())
    {
      return fail(std::move(unbind_result.unwrap_err()));
    }
    window->json_binding_handlers.erase(name_key);
    window->json_binding_contexts.erase(name_key);
    return {};
  }

  auto bind_window_menu_button(Window window, const Function<void> &callback) -> Result<void>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }

    window->menu_button_callback = callback;
    window->menu_button_callback_bound = true;
    return {};
  }

  auto unbind_window_menu_button(Window window) -> Result<void>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }

    window->menu_button_callback_bound = false;
    return {};
  }

} // namespace LaVista
