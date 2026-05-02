#include <LaVista/LaVista.hpp>

#include <webview/webview.h>

#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#if !defined(_WIN32)
#  include <unistd.h>
#endif

#include <auxid/containers/hash_map.hpp>

#if defined(_WIN32)
#  include <windows.h>
#  include <windowsx.h>
#  include <objbase.h>
#endif

#if defined(__linux__)
#  include <gtk/gtk.h>
#  if defined(GDK_WINDOWING_X11)
#    include <gdk/x11/gdkx.h>
#  endif
#endif

namespace LaVista
{
  namespace
  {
    auto webview_error_to_result(webview_error_t err, const char *context) -> Result<void>
    {
      if (WEBVIEW_SUCCEEDED(err))
      {
        return {};
      }
      return au::fail("%s failed with webview error code %d", context, static_cast<int>(err));
    }

    auto to_file_url(const std::filesystem::path &path) -> String
    {
      const String generic = String(std::filesystem::absolute(path).generic_string().c_str());
      String encoded;
      encoded.reserve(generic.size() + 8);
      for (const char c : generic)
      {
        switch (c)
        {
        case ' ':
          encoded += "%20";
          break;
        case '#':
          encoded += "%23";
          break;
        case '?':
          encoded += "%3F";
          break;
        default:
          encoded.push_back(c);
          break;
        }
      }
      return String("file://") + String(encoded.c_str());
    }

    auto read_file_utf8(const std::filesystem::path &path) -> Result<std::string>
    {
      std::ifstream in(path, std::ios::binary);
      if (!in)
      {
        return au::fail("Cannot open file: %s", path.string().c_str());
      }
      std::ostringstream ss;
      ss << in.rdbuf();
      return ss.str();
    }

    /* file:// documents resolve "/asset" to the filesystem root, not the bundle folder. Strip a leading '/' from
       root-relative URLs so they become relative to <base href>. Skip protocol-relative "//". */
    auto strip_root_relative_slash_after_prefix(std::string &html, const char *attr_prefix) -> void
    {
      const size_t prefix_len = std::strlen(attr_prefix);
      size_t pos = 0;
      while ((pos = html.find(attr_prefix, pos)) != std::string::npos)
      {
        const size_t slash_pos = pos + prefix_len;
        if (slash_pos >= html.size() || html[slash_pos] != '/')
        {
          pos += 1;
          continue;
        }
        if (slash_pos + 1 < html.size() && html[slash_pos + 1] == '/')
        {
          pos = slash_pos + 2;
          continue;
        }
        html.erase(slash_pos, 1);
        pos = slash_pos;
      }
    }

    /* Astro serializes string props as [0,"/path"]; in HTML that appears as [0,&quot;/path */
    auto strip_astro_serialized_root_paths(std::string &html) -> void
    {
      static constexpr const char *ASTRO_STR_PROP_PREFIX_FROM = "[0,&quot;/";
      static constexpr const char *ASTRO_STR_PROP_PREFIX_TO = "[0,&quot;";
      const size_t from_len = std::strlen(ASTRO_STR_PROP_PREFIX_FROM);
      const size_t to_len = std::strlen(ASTRO_STR_PROP_PREFIX_TO);
      size_t pos = 0;
      while ((pos = html.find(ASTRO_STR_PROP_PREFIX_FROM, pos)) != std::string::npos)
      {
        html.replace(pos, from_len, ASTRO_STR_PROP_PREFIX_TO);
        pos += to_len;
      }
    }

    auto inject_base_and_rewrite_spa_root_paths(std::string html, const std::filesystem::path &bundle_dir_abs)
        -> std::string
    {
      const String base_dir_url = to_file_url(bundle_dir_abs);
      const std::string base_tag = std::string("<base href=\"") + base_dir_url.c_str() + std::string("/\">");

      const size_t head_start = html.find("<head");
      if (head_start != std::string::npos)
      {
        const size_t head_gt = html.find('>', head_start);
        if (head_gt != std::string::npos)
        {
          html.insert(head_gt + 1, base_tag);
        }
      }

      strip_root_relative_slash_after_prefix(html, "href=\"");
      strip_root_relative_slash_after_prefix(html, "src=\"");
      strip_root_relative_slash_after_prefix(html, "action=\"");
      strip_root_relative_slash_after_prefix(html, "srcset=\"");
      strip_root_relative_slash_after_prefix(html, "component-url=\"");
      strip_root_relative_slash_after_prefix(html, "renderer-url=\"");
      strip_root_relative_slash_after_prefix(html, "before-hydration-url=\"");
      strip_astro_serialized_root_paths(html);

      return html;
    }

#if defined(_WIN32)
    /* Reserved name (RFC 2606) — wide + narrow must stay in sync. */
    static constexpr wchar_t SPA_VIRTUAL_HOST_NAME_W[] = L"lavista.bundle.invalid";
    static constexpr char SPA_VIRTUAL_HOST_NAME[] = "lavista.bundle.invalid";

    static auto apply_webview2_default_background(webview_t w) -> void
    {
      if (w == nullptr)
      {
        return;
      }
      void *const raw = webview_get_native_handle(w, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER);
      if (raw == nullptr)
      {
        return;
      }
      auto *const controller = static_cast<ICoreWebView2Controller *>(raw);
      ICoreWebView2Controller2 *controller2 = nullptr;
      if (FAILED(controller->QueryInterface(IID_ICoreWebView2Controller2, reinterpret_cast<void **>(&controller2))) ||
          controller2 == nullptr)
      {
        return;
      }
      /* WebView2 defaults to white; a thin seam often appears at the top of the control. Opaque slate (#475569). */
      COREWEBVIEW2_COLOR bg{};
      bg.A = 255;
      bg.R = 0x47;
      bg.G = 0x55;
      bg.B = 0x69;
      (void) controller2->put_DefaultBackgroundColor(bg);
      controller2->Release();
    }

    /* Map a fixed https host to the bundle directory so root-relative /_astro/... URLs resolve (NavigateToString
     * cannot load file:// or base-linked local assets reliably). */
    static auto map_webview2_spa_virtual_host(webview_t w, const std::filesystem::path &bundle_dir_abs) -> Result<void>
    {
      if (w == nullptr)
      {
        return au::fail("WebView is null");
      }
      void *const raw = webview_get_native_handle(w, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER);
      if (raw == nullptr)
      {
        return au::fail("WebView2 controller handle unavailable");
      }
      auto *const controller = static_cast<ICoreWebView2Controller *>(raw);
      ICoreWebView2 *core = nullptr;
      if (FAILED(controller->get_CoreWebView2(&core)) || core == nullptr)
      {
        return au::fail("get_CoreWebView2 failed");
      }
      ICoreWebView2_3 *core3 = nullptr;
      if (FAILED(core->QueryInterface(IID_ICoreWebView2_3, reinterpret_cast<void **>(&core3))) || core3 == nullptr)
      {
        core->Release();
        return au::fail("ICoreWebView2_3 is not available; update the WebView2 runtime");
      }
      const std::wstring folder = bundle_dir_abs.lexically_normal().wstring();
      if (folder.size() >= static_cast<size_t>(MAX_PATH))
      {
        core3->Release();
        core->Release();
        return au::fail("SPA bundle path is too long (MAX_PATH)");
      }
      const HRESULT hr = core3->SetVirtualHostNameToFolderMapping(SPA_VIRTUAL_HOST_NAME_W, folder.c_str(),
                                                                  COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY);
      core3->Release();
      core->Release();
      if (FAILED(hr))
      {
        return au::fail("SetVirtualHostNameToFolderMapping failed (HRESULT 0x%lX)", static_cast<unsigned long>(hr));
      }
      return {};
    }
#endif
  } // namespace

#if defined(__linux__)
  static auto ensure_gtk_initialized() -> Result<void>
  {
    static bool initialized = false;
    if (!initialized)
    {
      if (!gtk_init_check())
      {
        return au::fail("gtk_init_check() failed");
      }
      initialized = true;
    }
    return {};
  }
#endif

#if defined(_WIN32)
  namespace
  {
    struct Win32MonitorRaw
    {
      RECT rc{};
      bool primary = false;
    };

    static BOOL CALLBACK win32_monitor_enum_proc(HMONITOR hmonitor, HDC, LPRECT, LPARAM userdata)
    {
      auto *const list = reinterpret_cast<Vec<Win32MonitorRaw> *>(userdata);
      MONITORINFO mi{};
      mi.cbSize = sizeof(MONITORINFO);
      if (!GetMonitorInfo(hmonitor, &mi))
      {
        return TRUE;
      }
      Win32MonitorRaw row{};
      row.rc = mi.rcMonitor;
      row.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
      list->push_back(row);
      return TRUE;
    }

    static auto win32_collect_displays() -> Result<Vec<DisplayInfo>>
    {
      Vec<Win32MonitorRaw> raw;
      if (EnumDisplayMonitors(nullptr, nullptr, win32_monitor_enum_proc, reinterpret_cast<LPARAM>(&raw)) == 0)
      {
        return au::fail("EnumDisplayMonitors() failed");
      }
      std::sort(raw.begin(), raw.end(), [](const Win32MonitorRaw &a, const Win32MonitorRaw &b) {
        if (a.primary != b.primary)
        {
          return a.primary;
        }
        if (a.rc.top != b.rc.top)
        {
          return a.rc.top < b.rc.top;
        }
        return a.rc.left < b.rc.left;
      });

      Vec<DisplayInfo> out;
      out.reserve(raw.size());
      for (usize i = 0; i < raw.size(); ++i)
      {
        DisplayInfo d{};
        d.index = static_cast<i32>(i);
        d.x = static_cast<i32>(raw[i].rc.left);
        d.y = static_cast<i32>(raw[i].rc.top);
        d.width = static_cast<i32>(raw[i].rc.right - raw[i].rc.left);
        d.height = static_cast<i32>(raw[i].rc.bottom - raw[i].rc.top);
        out.push_back(d);
      }
      return out;
    }
  } // namespace
#elif defined(__linux__)
  namespace
  {
    struct LinuxMonitorRaw
    {
      GdkRectangle rect{};
      bool primary = false;
    };

    static void linux_gtk_window_move(GtkWindow *window, int x, int y)
    {
#if defined(GDK_WINDOWING_X11)
      GdkSurface *const surface = gtk_native_get_surface(GTK_NATIVE(window));
      if (surface == nullptr || !GDK_IS_X11_SURFACE(surface))
      {
        return;
      }
      GdkDisplay *const gdk_display = gdk_surface_get_display(surface);
      ::Display *const xdisplay = gdk_x11_display_get_xdisplay(gdk_display);
      const ::Window xwin = gdk_x11_surface_get_xid(surface);
      if (xdisplay == nullptr || xwin == 0)
      {
        return;
      }
      XMoveWindow(xdisplay, xwin, x, y);
#else
      (void)window;
      (void)x;
      (void)y;
#endif
    }

    static auto linux_collect_displays() -> Result<Vec<DisplayInfo>>
    {
      auto gtk_init_result = ensure_gtk_initialized();
      if (gtk_init_result.is_err())
      {
        return au::fail(std::move(gtk_init_result.unwrap_err()));
      }

      GdkDisplay *const display = gdk_display_get_default();
      if (display == nullptr)
      {
        return au::fail("gdk_display_get_default() returned null");
      }

      GListModel *const monitors = gdk_display_get_monitors(display);
      if (monitors == nullptr)
      {
        return au::fail("gdk_display_get_monitors() returned null");
      }

      GdkMonitor *primary_mon = nullptr;
#if defined(GDK_WINDOWING_X11)
      if (GDK_IS_X11_DISPLAY(display))
      {
        primary_mon = gdk_x11_display_get_primary_monitor(display);
      }
#endif

      const guint n = g_list_model_get_n_items(monitors);
      Vec<LinuxMonitorRaw> raw;
      raw.reserve(static_cast<usize>(n));
      for (guint i = 0; i < n; ++i)
      {
        gpointer const item = g_list_model_get_item(monitors, i);
        if (item == nullptr)
        {
          continue;
        }
        GdkMonitor *const mon = GDK_MONITOR(item);
        GdkRectangle r{};
        gdk_monitor_get_geometry(mon, &r);
        LinuxMonitorRaw row{};
        row.rect = r;
        row.primary = (primary_mon != nullptr && mon == primary_mon);
        g_object_unref(G_OBJECT(item));
        raw.push_back(row);
      }

      std::sort(raw.begin(), raw.end(), [](const LinuxMonitorRaw &a, const LinuxMonitorRaw &b) {
        if (a.primary != b.primary)
        {
          return a.primary;
        }
        if (a.rect.y != b.rect.y)
        {
          return a.rect.y < b.rect.y;
        }
        return a.rect.x < b.rect.x;
      });

      Vec<DisplayInfo> out;
      out.reserve(raw.size());
      for (usize i = 0; i < raw.size(); ++i)
      {
        DisplayInfo d{};
        d.index = static_cast<i32>(i);
        d.x = static_cast<i32>(raw[i].rect.x);
        d.y = static_cast<i32>(raw[i].rect.y);
        d.width = static_cast<i32>(raw[i].rect.width);
        d.height = static_cast<i32>(raw[i].rect.height);
        out.push_back(d);
      }
      return out;
    }
  } // namespace
#endif

  struct BindingContext_T
  {
    Window window = nullptr;
    String event;
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
    bool drag_strip_js_installed = false;
    bool running = true;
    HashMap<String, std::function<void(const String &)>> callbacks;
    HashMap<String, std::unique_ptr<BindingContext_T>> binding_contexts;

#if defined(_WIN32)
    HWND hwnd = nullptr;
#endif

#if defined(__linux__)
    GtkWindow *window = nullptr;
#endif
  };

#if defined(_WIN32)
  static auto ensure_window_class_registered() -> Result<void>
  {
    static bool registered = false;
    if (registered)
    {
      return {};
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"LaVistaWindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    /* Avoid default COLOR_WINDOW fill showing through as a bright line above the webview. */
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpfnWndProc = +[](HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) -> LRESULT {
      Window state = reinterpret_cast<Window>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
      switch (msg)
      {
      case WM_NCHITTEST: {
        /* Whole window is client: no DWM edge resize (HTTOP/HTBOTTOM/…) or top strip hit-testing. */
        POINT pt{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
        RECT wr{};
        if (!GetWindowRect(hwnd, &wr))
        {
          return DefWindowProcW(hwnd, msg, w_param, l_param);
        }
        if (!PtInRect(&wr, pt))
        {
          return HTNOWHERE;
        }
        return HTCLIENT;
      }
      case WM_ERASEBKGND:
        return 1;
      case WM_SIZE: {
        if (state != nullptr && state->webview != nullptr)
        {
          HWND widget =
              static_cast<HWND>(webview_get_native_handle(state->webview, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
          if (widget != nullptr)
          {
            RECT r{};
            if (GetClientRect(hwnd, &r))
            {
              MoveWindow(widget, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
            }
          }
        }
        return 0;
      }
      case WM_MOVE: {
        if (state != nullptr)
        {
          RECT r{};
          if (GetWindowRect(hwnd, &r))
          {
            state->x = static_cast<i32>(r.left);
            state->y = static_cast<i32>(r.top);
          }
        }
        return DefWindowProcW(hwnd, msg, w_param, l_param);
      }
      case WM_CLOSE: {
        DestroyWindow(hwnd);
        return 0;
      }
      case WM_DESTROY: {
        if (state != nullptr)
        {
          state->running = false;
        }
        return 0;
      }
      case WM_NCCALCSIZE: {
        if (w_param == TRUE)
        {
          return 0;
        }
        return DefWindowProcW(hwnd, msg, w_param, l_param);
      }
      default:
        return DefWindowProcW(hwnd, msg, w_param, l_param);
      }
    };

    if (RegisterClassExW(&wc) == 0)
    {
      return au::fail("RegisterClassExW() failed");
    }

    registered = true;
    return {};
  }
#endif

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
        webview_error_to_result(webview_bind(
                                    window->webview, "startWindowDrag",
                                    +[](const char *id, const char *req, void *arg) {
                                      (void) req;
                                      auto *window_state = static_cast<Window>(arg);
                                      if (window_state == nullptr)
                                      {
                                        return;
                                      }

#if defined(_WIN32)
                                      if (window_state->hwnd != nullptr && IsWindow(window_state->hwnd))
                                      {
                                        ReleaseCapture();
                                        SendMessageW(window_state->hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
                                      }
#elif defined(__linux__)
                                      if (window_state->window != nullptr)
                                      {
                                        GdkSurface *surface =
                                            gtk_native_get_surface(GTK_NATIVE(window_state->window));
                                        GdkDisplay *display =
                                            gtk_widget_get_display(GTK_WIDGET(window_state->window));
                                        GdkSeat *seat =
                                            display != nullptr ? gdk_display_get_default_seat(display) : nullptr;
                                        GdkDevice *pointer = seat != nullptr ? gdk_seat_get_pointer(seat) : nullptr;
                                        double x = 0.0;
                                        double y = 0.0;
                                        if (surface != nullptr && pointer != nullptr && GDK_IS_TOPLEVEL(surface))
                                        {
                                          gdk_surface_get_device_position(surface, pointer, &x, &y, nullptr);
                                          gdk_toplevel_begin_move(GDK_TOPLEVEL(surface), pointer, 1, x, y,
                                                                  GDK_CURRENT_TIME);
                                        }
                                      }
#endif

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

    auto init_result = webview_error_to_result(webview_init(window->webview, init_buf), "webview_init(drag-strip)");
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
    return webview_error_to_result(webview_eval(window->webview, js.c_str()), "webview_eval(drag-strip)");
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
        return webview_error_to_result(
            webview_eval(window->webview,
                         "if(window.__lavistaDragStrip){"
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

  auto get_displays() -> Result<Vec<DisplayInfo>>
  {
#if defined(_WIN32)
    return win32_collect_displays();
#elif defined(__linux__)
    return linux_collect_displays();
#else
    return au::fail("get_displays is not available on this platform");
#endif
  }

  auto create_window(const WindowCreateOptions &options, const WindowDragStripOptions &drag_strip_options) -> Result<Window>
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
#if defined(_WIN32) || defined(__linux__)
    {
      AU_TRY_VAR(displays, get_displays());
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
#endif

    auto state = std::make_unique<Window_T>();
    state->title = title;
    state->width = width;
    state->height = height;
    state->x = window_x;
    state->y = window_y;

#if defined(_WIN32)
    auto register_result = ensure_window_class_registered();
    if (register_result.is_err())
    {
      return au::fail(std::move(register_result.unwrap_err()));
    }

    HWND hwnd = CreateWindowExW(
        0, L"LaVistaWindowClass", L"", WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, window_x, window_y,
        static_cast<int>(width), static_cast<int>(height), nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (hwnd == nullptr)
    {
      return au::fail("CreateWindowExW() failed");
    }

    state->hwnd = hwnd;
    state->webview = webview_create(0, hwnd);
    if (state->webview == nullptr)
    {
      DestroyWindow(hwnd);
      return au::fail("webview_create() failed");
    }

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state.get()));

    HWND widget = static_cast<HWND>(webview_get_native_handle(state->webview, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
    if (widget != nullptr)
    {
      RECT r{};
      if (GetClientRect(hwnd, &r))
      {
        MoveWindow(widget, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
      }
    }
#elif defined(__linux__)
    auto gtk_init_result = ensure_gtk_initialized();
    if (gtk_init_result.is_err())
    {
      return au::fail(std::move(gtk_init_result.unwrap_err()));
    }

    GtkWidget *window_widget = gtk_window_new();
    gtk_window_set_default_size(GTK_WINDOW(window_widget), width, height);
    gtk_widget_set_visible(window_widget, TRUE);
    gtk_window_set_title(GTK_WINDOW(window_widget), title.c_str());
    gtk_window_set_decorated(GTK_WINDOW(window_widget), FALSE);

    state->window = GTK_WINDOW(window_widget);
    state->webview = webview_create(0, window_widget);
    if (state->webview == nullptr)
    {
      gtk_window_destroy(GTK_WINDOW(window_widget));
      return au::fail("webview_create() failed");
    }

    g_signal_connect(G_OBJECT(window_widget), "destroy", G_CALLBACK(+[](GtkWidget *, gpointer user_data) {
                       auto *window_state = static_cast<Window>(user_data);
                       if (window_state != nullptr)
                       {
                         window_state->running = false;
                         if (window_state->webview != nullptr)
                         {
                           webview_terminate(window_state->webview);
                         }
                       }
                     }),
                     state.get());

    linux_gtk_window_move(GTK_WINDOW(window_widget), static_cast<int>(window_x), static_cast<int>(window_y));
#else
    state->webview = webview_create(0, nullptr);
    if (state->webview == nullptr)
    {
      return au::fail("webview_create() failed");
    }
#endif

    auto set_title_result =
        webview_error_to_result(webview_set_title(state->webview, title.c_str()), "webview_set_title");
    if (set_title_result.is_err())
    {
      return au::fail(std::move(set_title_result.unwrap_err()));
    }
#if defined(__linux__)
    /* webview GTK: set_size_impl returns INVALID_ARGUMENT unconditionally (missing return in upstream webview.h).
       Apply size on our GtkWindow instead (GTK 4 has no gtk_window_resize). */
    if (state->window != nullptr)
    {
      const int w = static_cast<int>(width);
      const int h = static_cast<int>(height);
      gtk_window_set_default_size(state->window, w, h);
      gtk_widget_set_size_request(GTK_WIDGET(state->window), w, h);
    }
#else
    auto set_size_result =
        webview_error_to_result(webview_set_size(state->webview, width, height, WEBVIEW_HINT_NONE), "webview_set_size");
    if (set_size_result.is_err())
    {
      return au::fail(std::move(set_size_result.unwrap_err()));
    }
#  if defined(_WIN32)
    apply_webview2_default_background(state->webview);
#  endif
#endif

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

#if defined(_WIN32)
    /* HTTPS virtual host → folder: production builds keep root-relative URLs; avoids NavigateToString file blocking. */
    auto map_result = map_webview2_spa_virtual_host(window->webview, bundle_dir_abs);
    if (map_result.is_err())
    {
      (void) destroy_window(window);
      return au::fail(std::move(map_result.unwrap_err()));
    }

    const std::string entry_name = index_html.filename().generic_string();
    const std::string vhost_url = std::string("https://") + SPA_VIRTUAL_HOST_NAME + "/" + entry_name;
    auto nav_result =
        webview_error_to_result(webview_navigate(window->webview, vhost_url.c_str()), "webview_navigate(spa bundle)");
    if (nav_result.is_err())
    {
      (void) destroy_window(window);
      return au::fail(std::move(nav_result.unwrap_err()));
    }
#else
    /* GTK/macOS: load real file:// document so subresources resolve; NavigateToString blocks local assets. */
    AU_TRY_VAR(html_raw, read_file_utf8(index_html));
    const std::string html = inject_base_and_rewrite_spa_root_paths(std::move(html_raw), bundle_dir_abs);

    std::filesystem::path temp_html = std::filesystem::temp_directory_path();
    temp_html /= std::string("lavista-spa-") + std::to_string(static_cast<unsigned long long>(getpid())) + ".html";
    {
      std::ofstream out(temp_html, std::ios::binary | std::ios::trunc);
      if (!out)
      {
        (void) destroy_window(window);
        return au::fail("Cannot write temporary SPA HTML: %s", temp_html.string().c_str());
      }
      out << html;
    }

    const String file_url = to_file_url(temp_html);
    auto nav_result =
        webview_error_to_result(webview_navigate(window->webview, file_url.c_str()), "webview_navigate(spa bundle)");
    if (nav_result.is_err())
    {
      (void) destroy_window(window);
      std::error_code ec;
      std::filesystem::remove(temp_html, ec);
      return au::fail(std::move(nav_result.unwrap_err()));
    }
#endif

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

    auto destroy_result = webview_error_to_result(webview_destroy(window->webview), "webview_destroy");
    window->webview = nullptr;
    window->running = false;

#if defined(_WIN32)
    if (window->hwnd != nullptr && IsWindow(window->hwnd))
    {
      DestroyWindow(window->hwnd);
    }
    window->hwnd = nullptr;
#endif

    delete window;
    return destroy_result;
  }

  auto update_window(Window window) -> bool
  {
    if (window == nullptr || !window->running || window->webview == nullptr)
      return false;

#if defined(_WIN32)
    if (window->hwnd == nullptr || !IsWindow(window->hwnd))
      return false;

    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    if (window->hwnd != nullptr && IsWindow(window->hwnd))
    {
      RECT r{};
      if (GetWindowRect(window->hwnd, &r))
      {
        window->x = static_cast<i32>(r.left);
        window->y = static_cast<i32>(r.top);
        window->width = static_cast<i32>(r.right - r.left);
        window->height = static_cast<i32>(r.bottom - r.top);
      }
    }
#elif defined(__linux__)
    if (window->window == nullptr)
      return false;
    g_main_context_iteration(nullptr, FALSE);
#endif

    return window->running;
  }

  auto get_window_size(Window window) -> Result<Pair<i32, i32>>
  {
    if (window == nullptr)
    {
      return au::fail("Window is null");
    }

#if defined(_WIN32)
    if (window->hwnd != nullptr && IsWindow(window->hwnd))
    {
      RECT r{};
      if (GetWindowRect(window->hwnd, &r))
      {
        window->width = static_cast<i32>(r.right - r.left);
        window->height = static_cast<i32>(r.bottom - r.top);
      }
    }
#endif
    return Pair<i32, i32>{window->width, window->height};
  }

  auto get_window_position(Window window) -> Result<Pair<i32, i32>>
  {
    if (window == nullptr)
    {
      return au::fail("Window is null");
    }

#if defined(_WIN32)
    if (window->hwnd != nullptr && IsWindow(window->hwnd))
    {
      RECT r{};
      if (GetWindowRect(window->hwnd, &r))
      {
        window->x = static_cast<i32>(r.left);
        window->y = static_cast<i32>(r.top);
      }
    }
#endif
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
        webview_error_to_result(webview_set_title(window->webview, title.c_str()), "webview_set_title");
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
#if defined(__linux__)
    if (window->window == nullptr)
    {
      return au::fail("Native window is null");
    }
    {
      const int w = static_cast<int>(width);
      const int h = static_cast<int>(height);
      gtk_window_set_default_size(window->window, w, h);
      gtk_widget_set_size_request(GTK_WIDGET(window->window), w, h);
    }
#else
    auto set_size_result = webview_error_to_result(webview_set_size(window->webview, width, height, WEBVIEW_HINT_NONE),
                                                   "webview_set_size");
    if (set_size_result.is_err())
    {
      return au::fail(std::move(set_size_result.unwrap_err()));
    }
#endif
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

#if defined(_WIN32)
    if (window->hwnd == nullptr || !IsWindow(window->hwnd))
    {
      return au::fail("Native window handle is not valid");
    }

    RECT r{};
    if (!GetWindowRect(window->hwnd, &r))
    {
      return au::fail("GetWindowRect() failed");
    }
    const int width = r.right - r.left;
    const int height = r.bottom - r.top;
    if (!SetWindowPos(window->hwnd, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER))
    {
      return au::fail("SetWindowPos() failed");
    }
#elif defined(__linux__)
    if (window->window != nullptr)
    {
      linux_gtk_window_move(window->window, x, y);
    }
#endif
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

    auto bind_result = webview_error_to_result(
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

    auto unbind_result = webview_error_to_result(webview_unbind(window->webview, event.c_str()), "webview_unbind");
    if (unbind_result.is_err())
    {
      return au::fail(std::move(unbind_result.unwrap_err()));
    }
    window->callbacks.erase(event);
    window->binding_contexts.erase(event);
    return {};
  }

} // namespace LaVista
