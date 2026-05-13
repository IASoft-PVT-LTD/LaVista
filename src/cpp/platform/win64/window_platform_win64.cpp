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
#include <LaVista_webview2.hpp>

#include <windows.h>
#include <windowsx.h>

#include <objbase.h>

#if defined(__clang__)
#  ifndef interface
#    define interface struct
#  endif
#endif

#include <WebView2.h>

namespace LaVista::_internal
{
  namespace
  {
    /* put_Bounds on top of MoveWindow double-applies layout and caused a visible gap; only MoveWindow the child
     * HWNDs. After moving, tell WebView2 the parent (re)positioned so hit-testing and drags match the widget. */
    static auto win64_notify_webview_parent_moved(webview_t w) -> void
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
      (void) controller->NotifyParentWindowPositionChanged();
    }

    static auto win64_create_color_dib32(const u8 *rgba, int width, int height) -> HBITMAP
    {
      BITMAPINFO bmi{};
      bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = width;
      bmi.bmiHeader.biHeight = -height;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;
      void *bits = nullptr;
      HDC hdc = GetDC(nullptr);
      HBITMAP const hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
      ReleaseDC(nullptr, hdc);
      if (hbmp == nullptr || bits == nullptr)
      {
        return nullptr;
      }
      auto *const px = static_cast<u8 *>(bits);
      for (int y = 0; y < height; ++y)
      {
        for (int x = 0; x < width; ++x)
        {
          const u8 *const s = rgba + (static_cast<usize>(y * width + x) * 4);
          u8 *const d = px + (static_cast<usize>(y * width + x) * 4);
          d[0] = s[2];
          d[1] = s[1];
          d[2] = s[0];
          d[3] = s[3];
        }
      }
      return hbmp;
    }

    static auto win64_create_and_mask_bitmap(const u8 *rgba, int width, int height) -> HBITMAP
    {
      const int row_bytes = ((width + 31) / 32) * 4;
      Vec<u8> bits(static_cast<usize>(row_bytes * height), 0);
      for (int y = 0; y < height; ++y)
      {
        for (int x = 0; x < width; ++x)
        {
          const u8 a = rgba[static_cast<usize>((y * width + x) * 4 + 3)];
          if (a == 0)
          {
            bits[static_cast<usize>(y * row_bytes + x / 8)] |= static_cast<u8>(1 << (7 - (x % 8)));
          }
        }
      }
      return CreateBitmap(width, height, 1, 1, bits.data());
    }

    static auto win64_create_hicon_from_rgba(const u8 *rgba, int width, int height) -> HICON
    {
      HBITMAP const color = win64_create_color_dib32(rgba, width, height);
      if (color == nullptr)
      {
        return nullptr;
      }
      HBITMAP const mask = win64_create_and_mask_bitmap(rgba, width, height);
      if (mask == nullptr)
      {
        DeleteObject(color);
        return nullptr;
      }
      ICONINFO ii{};
      ii.fIcon = TRUE;
      ii.xHotspot = 0;
      ii.yHotspot = 0;
      ii.hbmMask = mask;
      ii.hbmColor = color;
      HICON const icon = CreateIconIndirect(&ii);
      DeleteObject(mask);
      DeleteObject(color);
      return icon;
    }

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
      wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
      wc.lpfnWndProc = +[](HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) -> LRESULT {
        auto *const state = reinterpret_cast<Window>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (msg)
        {
        case WM_NCHITTEST: {
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
          if (state != nullptr)
          {
            _internal::platform_layout_webviews(state);
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
        return fail("RegisterClassExW() failed");
      }

      registered = true;
      return {};
    }
  } // namespace

  auto platform_apply_post_webview_setup(Window_T &state, i32 width, i32 height) -> Result<void>
  {
    /* Title bar webview is created only after create_window; during setup only the content webview exists.
       Always pass the full window dimensions here — webview_set_size with a reduced height is interpreted as the
       outer window size on Win32 and fights WM_SIZE when we stack a title bar via MoveWindow. */
    auto set_main_result =
        webview_error_to_result(webview_set_size(state.webview, width, height, WEBVIEW_HINT_NONE), "webview_set_size");
    if (set_main_result.is_err())
    {
      return fail(std::move(set_main_result.unwrap_err()));
    }
    apply_webview2_default_background(state.webview);
    platform_layout_webviews(static_cast<Window>(&state));
    return {};
  }

  auto platform_create_window(Window_T &state, i32 width, i32 height, i32 window_x, i32 window_y, const String &title,
                              const String &icon_path) -> Result<void>
  {
    (void) title;

    auto register_result = ensure_window_class_registered();
    if (register_result.is_err())
    {
      return fail(std::move(register_result.unwrap_err()));
    }

    HWND hwnd = CreateWindowExW(
        0, L"LaVistaWindowClass", L"", WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, window_x, window_y,
        static_cast<int>(width), static_cast<int>(height), nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (hwnd == nullptr)
    {
      return fail("CreateWindowExW() failed");
    }

    {
      Vec<u8> rgba_full;
      i32 iw = 0;
      i32 ih = 0;
      auto decode = utils::load_image_rgba_from_file(icon_path, rgba_full, iw, ih);
      if (decode.is_err())
      {
        DestroyWindow(hwnd);
        return fail(std::move(decode.unwrap_err()));
      }
      Vec<u8> rgba32;
      Vec<u8> rgba16;
      utils::resize_rgba_nearest(rgba_full.data(), iw, ih, 32, 32, rgba32);
      utils::resize_rgba_nearest(rgba_full.data(), iw, ih, 16, 16, rgba16);

      HICON const sm = win64_create_hicon_from_rgba(rgba16.data(), 16, 16);
      HICON const bg = win64_create_hicon_from_rgba(rgba32.data(), 32, 32);
      if (sm == nullptr || bg == nullptr)
      {
        if (sm != nullptr)
        {
          DestroyIcon(sm);
        }
        if (bg != nullptr)
        {
          DestroyIcon(bg);
        }
        DestroyWindow(hwnd);
        return fail("Failed to build window icon (CreateIconIndirect)");
      }
      (void) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(sm));
      (void) SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bg));
      state.platform.icon_small = sm;
      state.platform.icon_big = bg;
    }

    state.platform.hwnd = hwnd;
    state.webview = webview_create(0, hwnd);
    if (state.webview == nullptr)
    {
      if (state.platform.icon_big != nullptr)
      {
        DestroyIcon(state.platform.icon_big);
        state.platform.icon_big = nullptr;
      }
      if (state.platform.icon_small != nullptr)
      {
        DestroyIcon(state.platform.icon_small);
        state.platform.icon_small = nullptr;
      }
      DestroyWindow(hwnd);
      state.platform.hwnd = nullptr;
      return fail("webview_create() failed");
    }

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));

    HWND widget = static_cast<HWND>(webview_get_native_handle(state.webview, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
    if (widget != nullptr)
    {
      RECT r{};
      if (GetClientRect(hwnd, &r))
      {
        MoveWindow(widget, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
      }
    }
    return {};
  }

  auto platform_destroy_native(Window window) -> void
  {
    if (window->platform.hwnd != nullptr && IsWindow(window->platform.hwnd))
    {
      HWND const hwnd = window->platform.hwnd;
      if (window->platform.icon_small != nullptr || window->platform.icon_big != nullptr)
      {
        (void) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, 0);
        (void) SendMessageW(hwnd, WM_SETICON, ICON_BIG, 0);
      }
      DestroyWindow(hwnd);
    }
    if (window->platform.icon_big != nullptr)
    {
      DestroyIcon(window->platform.icon_big);
      window->platform.icon_big = nullptr;
    }
    if (window->platform.icon_small != nullptr)
    {
      DestroyIcon(window->platform.icon_small);
      window->platform.icon_small = nullptr;
    }
    window->platform.hwnd = nullptr;
  }

  auto platform_pump_events(Window window) -> bool
  {
    if (window->platform.hwnd == nullptr || !IsWindow(window->platform.hwnd))
    {
      return false;
    }

    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    return window->platform.hwnd != nullptr && IsWindow(window->platform.hwnd);
  }

  auto platform_sync_window_frame_from_native(Window window) -> void
  {
    if (window->platform.hwnd != nullptr && IsWindow(window->platform.hwnd))
    {
      RECT r{};
      if (GetWindowRect(window->platform.hwnd, &r))
      {
        window->x = static_cast<i32>(r.left);
        window->y = static_cast<i32>(r.top);
        window->width = static_cast<i32>(r.right - r.left);
        window->height = static_cast<i32>(r.bottom - r.top);
      }
    }
  }

  auto platform_set_window_position(Window window, i32 x, i32 y) -> Result<void>
  {
    if (window->platform.hwnd == nullptr || !IsWindow(window->platform.hwnd))
    {
      return fail("Native window handle is not valid");
    }

    RECT r{};
    if (!GetWindowRect(window->platform.hwnd, &r))
    {
      return fail("GetWindowRect() failed");
    }
    const int width = r.right - r.left;
    const int height = r.bottom - r.top;
    if (!SetWindowPos(window->platform.hwnd, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER))
    {
      return fail("SetWindowPos() failed");
    }
    return {};
  }

  auto platform_set_window_size(Window window, i32 width, i32 height) -> Result<void>
  {
    if (window->platform.hwnd == nullptr || !IsWindow(window->platform.hwnd))
    {
      return fail("Native window handle is not valid");
    }
    /* Resize the frame once at full dimensions; children are positioned in platform_layout_webviews only. */
    if (SetWindowPos(window->platform.hwnd, nullptr, 0, 0, static_cast<int>(width), static_cast<int>(height),
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) == FALSE)
    {
      return fail("SetWindowPos() failed");
    }
    platform_layout_webviews(window);
    return {};
  }

  auto platform_create_titlebar_webview(Window window) -> Result<void>
  {
    if (window->titlebar_webview != nullptr)
    {
      return {};
    }
    if (window->platform.hwnd == nullptr || !IsWindow(window->platform.hwnd))
    {
      return fail("Native window handle is not valid");
    }
    window->titlebar_webview = webview_create(0, window->platform.hwnd);
    if (window->titlebar_webview == nullptr)
    {
      return fail("titlebar webview_create() failed");
    }
    apply_webview2_default_background(window->titlebar_webview);
    return {};
  }

  auto platform_destroy_titlebar_webview(Window window) -> Result<void>
  {
    if (window->titlebar_webview == nullptr)
    {
      return {};
    }
    auto r = webview_error_to_result(webview_destroy(window->titlebar_webview), "webview_destroy(titlebar)");
    window->titlebar_webview = nullptr;
    return r;
  }

  auto platform_layout_webviews(Window window) -> void
  {
    if (window->platform.hwnd == nullptr || !IsWindow(window->platform.hwnd))
    {
      return;
    }
    RECT r{};
    if (!GetClientRect(window->platform.hwnd, &r))
    {
      return;
    }
    const int total_w = r.right - r.left;
    const int total_h = r.bottom - r.top;
    int tb_h = 0;
    if (window->titlebar_webview != nullptr && window->titlebar_height_px > 0)
    {
      tb_h = window->titlebar_height_px;
    }
    if (tb_h > total_h)
    {
      tb_h = total_h;
    }
    const int content_h = total_h - tb_h;

    if (window->titlebar_webview != nullptr && tb_h > 0)
    {
      HWND tb_widget =
          static_cast<HWND>(webview_get_native_handle(window->titlebar_webview, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
      if (tb_widget != nullptr)
      {
        MoveWindow(tb_widget, 0, 0, total_w, tb_h, TRUE);
      }
      win64_notify_webview_parent_moved(window->titlebar_webview);
    }

    HWND main_widget =
        static_cast<HWND>(webview_get_native_handle(window->webview, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
    if (main_widget != nullptr)
    {
      MoveWindow(main_widget, 0, tb_h, total_w, content_h > 0 ? content_h : 1, TRUE);
    }
    win64_notify_webview_parent_moved(window->webview);
  }

  auto platform_start_window_drag(Window window) -> void
  {
    if (window->platform.hwnd == nullptr || !IsWindow(window->platform.hwnd))
    {
      return;
    }

    /* WM_SYSCOMMAND/SC_MOVE enters keyboard-driven move mode (arrow keys) and does not follow the mouse, so the
     * titlebar appears unresponsive. The correct pattern for a borderless window is to feign a non-client
     * caption click while the user's mouse button is still held: release whatever capture the WebView2 child took
     * on the preceding mousedown, then NCLBUTTONDOWN/HTCAPTION to kick off the system move loop, which tracks the
     * cursor until the button is released. Forward the current cursor position (GetMessagePos format matches
     * WM_NCLBUTTONDOWN's lParam exactly) so the move loop anchors where the user actually clicked. Post rather
     * than Send to avoid re-entering the WebView2 script thread. */
    ReleaseCapture();
    (void) PostMessageW(window->platform.hwnd, WM_NCLBUTTONDOWN, HTCAPTION, static_cast<LPARAM>(GetMessagePos()));
  }

  auto platform_minimize_window(Window window) -> void
  {
    if (window->platform.hwnd != nullptr && IsWindow(window->platform.hwnd))
    {
      ShowWindow(window->platform.hwnd, SW_MINIMIZE);
    }
  }

  auto platform_toggle_maximize_window(Window window) -> void
  {
    if (window->platform.hwnd != nullptr && IsWindow(window->platform.hwnd))
    {
      if (IsZoomed(window->platform.hwnd) != FALSE)
      {
        ShowWindow(window->platform.hwnd, SW_RESTORE);
      }
      else
      {
        ShowWindow(window->platform.hwnd, SW_MAXIMIZE);
      }
    }
  }

  auto platform_window_is_maximized(Window window) -> bool
  {
    if (window->platform.hwnd == nullptr || !IsWindow(window->platform.hwnd))
    {
      return false;
    }
    return IsZoomed(window->platform.hwnd) != FALSE;
  }

  auto platform_close_window(Window window) -> void
  {
    if (window->platform.hwnd != nullptr && IsWindow(window->platform.hwnd))
    {
      PostMessageW(window->platform.hwnd, WM_CLOSE, 0, 0);
    }
  }
} // namespace LaVista::_internal

namespace LaVista
{
  auto post_binary_data(Window window, const Span<const u8> &buffer) -> Result<void>
  {
    if (window == nullptr || window->webview == nullptr)
    {
      return fail("Window or webview is null");
    }

    void *const raw = webview_get_native_handle(window->webview, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER);
    if (raw == nullptr)
    {
      return fail("WebView2 controller handle unavailable");
    }

    auto *const controller = static_cast<ICoreWebView2Controller *>(raw);
    ICoreWebView2 *core = nullptr;
    if (FAILED(controller->get_CoreWebView2(&core)) || core == nullptr)
    {
      return fail("get_CoreWebView2 failed");
    }

    ICoreWebView2_17 *core17 = nullptr;
    if (FAILED(core->QueryInterface(IID_ICoreWebView2_17, reinterpret_cast<void **>(&core17))) || core17 == nullptr)
    {
      core->Release();
      return fail("ICoreWebView2_17 is not available; update the WebView2 runtime");
    }

    ICoreWebView2_2 *core2 = nullptr;
    if (FAILED(core->QueryInterface(IID_ICoreWebView2_2, reinterpret_cast<void **>(&core2))) || core2 == nullptr)
    {
      core17->Release();
      core->Release();
      return fail("ICoreWebView2_2 is not available");
    }

    ICoreWebView2Environment *env = nullptr;
    if (FAILED(core2->get_Environment(&env)) || env == nullptr)
    {
      core2->Release();
      core17->Release();
      core->Release();
      return fail("get_Environment failed");
    }

    ICoreWebView2Environment12 *env12 = nullptr;
    if (FAILED(env->QueryInterface(IID_ICoreWebView2Environment12, reinterpret_cast<void **>(&env12))) || env12 == nullptr)
    {
      env->Release();
      core2->Release();
      core17->Release();
      core->Release();
      return fail("ICoreWebView2Environment12 is not available; update the WebView2 runtime");
    }

    ICoreWebView2SharedBuffer *shared_buffer = nullptr;
    if (FAILED(env12->CreateSharedBuffer(buffer.size(), &shared_buffer)) || shared_buffer == nullptr)
    {
      env12->Release();
      env->Release();
      core2->Release();
      core17->Release();
      core->Release();
      return fail("CreateSharedBuffer failed");
    }

    BYTE *data = nullptr;
    if (FAILED(shared_buffer->get_Buffer(&data)) || data == nullptr)
    {
      shared_buffer->Release();
      env12->Release();
      env->Release();
      core2->Release();
      core17->Release();
      core->Release();
      return fail("get_Buffer failed");
    }

    std::memcpy(data, buffer.data(), buffer.size());

    if (FAILED(core17->PostSharedBufferToScript(shared_buffer, COREWEBVIEW2_SHARED_BUFFER_ACCESS_READ_ONLY, L"\"\"")))
    {
      shared_buffer->Release();
      env12->Release();
      env->Release();
      core2->Release();
      core17->Release();
      core->Release();
      return fail("PostSharedBufferToScript failed");
    }

    shared_buffer->Release();
    env12->Release();
    env->Release();
    core2->Release();
    core17->Release();
    core->Release();

    return {};
  }
} // namespace LaVista
