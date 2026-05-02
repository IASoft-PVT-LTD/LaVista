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

namespace LaVista::detail
{
  namespace
  {
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
  } // namespace

  auto platform_apply_post_webview_setup(Window_T &state, i32 width, i32 height) -> Result<void>
  {
    auto set_size_result =
        webview_error_to_result(webview_set_size(state.webview, width, height, WEBVIEW_HINT_NONE), "webview_set_size");
    if (set_size_result.is_err())
    {
      return au::fail(std::move(set_size_result.unwrap_err()));
    }
    apply_webview2_default_background(state.webview);
    return {};
  }

  auto platform_create_window(Window_T &state, i32 width, i32 height, i32 window_x, i32 window_y, const String &title)
      -> Result<void>
  {
    (void) title;

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

    state.platform.hwnd = hwnd;
    state.webview = webview_create(0, hwnd);
    if (state.webview == nullptr)
    {
      DestroyWindow(hwnd);
      state.platform.hwnd = nullptr;
      return au::fail("webview_create() failed");
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

  void platform_destroy_native(Window window)
  {
    if (window->platform.hwnd != nullptr && IsWindow(window->platform.hwnd))
    {
      DestroyWindow(window->platform.hwnd);
    }
    window->platform.hwnd = nullptr;
  }

  bool platform_pump_events(Window window)
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

  void platform_sync_window_frame_from_native(Window window)
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
      return au::fail("Native window handle is not valid");
    }

    RECT r{};
    if (!GetWindowRect(window->platform.hwnd, &r))
    {
      return au::fail("GetWindowRect() failed");
    }
    const int width = r.right - r.left;
    const int height = r.bottom - r.top;
    if (!SetWindowPos(window->platform.hwnd, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER))
    {
      return au::fail("SetWindowPos() failed");
    }
    return {};
  }

  auto platform_set_window_size(Window window, i32 width, i32 height) -> Result<void>
  {
    return webview_error_to_result(webview_set_size(window->webview, width, height, WEBVIEW_HINT_NONE),
                                   "webview_set_size");
  }

  void platform_start_window_drag(Window window)
  {
    if (window->platform.hwnd != nullptr && IsWindow(window->platform.hwnd))
    {
      ReleaseCapture();
      SendMessageW(window->platform.hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
    }
  }
} // namespace LaVista::detail
