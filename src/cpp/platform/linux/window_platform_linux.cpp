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
#include <LaVista_gtk_webview.hpp>

#include <gtk/gtk.h>
#if defined(GDK_WINDOWING_X11)
#  include <gdk/x11/gdkx.h>
#endif

namespace LaVista::detail
{
  namespace
  {
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
      (void) window;
      (void) x;
      (void) y;
#endif
    }
  } // namespace

  auto platform_apply_post_webview_setup(Window_T &state, i32 width, i32 height) -> Result<void>
  {
    if (state.platform.gtk_window != nullptr)
    {
      const int w = static_cast<int>(width);
      const int h = static_cast<int>(height);
      gtk_window_set_default_size(state.platform.gtk_window, w, h);
      gtk_widget_set_size_request(GTK_WIDGET(state.platform.gtk_window), w, h);
    }
    return {};
  }

  auto platform_create_window(Window_T &state, i32 width, i32 height, i32 window_x, i32 window_y, const String &title)
      -> Result<void>
  {
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

    state.platform.gtk_window = GTK_WINDOW(window_widget);
    state.webview = webview_create(0, window_widget);
    if (state.webview == nullptr)
    {
      gtk_window_destroy(GTK_WINDOW(window_widget));
      state.platform.gtk_window = nullptr;
      return au::fail("webview_create() failed");
    }

    g_signal_connect(G_OBJECT(window_widget), "destroy", G_CALLBACK(+[](GtkWidget *, gpointer user_data) {
                       auto *const window_state = reinterpret_cast<Window>(user_data);
                       if (window_state != nullptr)
                       {
                         window_state->running = false;
                         if (window_state->webview != nullptr)
                         {
                           webview_terminate(window_state->webview);
                         }
                       }
                     }),
                     reinterpret_cast<gpointer>(static_cast<Window>(std::addressof(state))));

    linux_gtk_window_move(GTK_WINDOW(window_widget), static_cast<int>(window_x), static_cast<int>(window_y));
    return {};
  }

  void platform_destroy_native(Window)
  {
  }

  bool platform_pump_events(Window window)
  {
    if (window->platform.gtk_window == nullptr)
    {
      return false;
    }
    g_main_context_iteration(nullptr, FALSE);
    return true;
  }

  void platform_sync_window_frame_from_native(Window)
  {
  }

  auto platform_set_window_position(Window window, i32 x, i32 y) -> Result<void>
  {
    if (window->platform.gtk_window != nullptr)
    {
      linux_gtk_window_move(window->platform.gtk_window, x, y);
    }
    return {};
  }

  auto platform_set_window_size(Window window, i32 width, i32 height) -> Result<void>
  {
    if (window->platform.gtk_window == nullptr)
    {
      return au::fail("Native window is null");
    }
    {
      const int w = static_cast<int>(width);
      const int h = static_cast<int>(height);
      gtk_window_set_default_size(window->platform.gtk_window, w, h);
      gtk_widget_set_size_request(GTK_WIDGET(window->platform.gtk_window), w, h);
    }
    return {};
  }

  void platform_start_window_drag(Window window)
  {
    if (window->platform.gtk_window != nullptr)
    {
      GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(window->platform.gtk_window));
      GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(window->platform.gtk_window));
      GdkSeat *seat = display != nullptr ? gdk_display_get_default_seat(display) : nullptr;
      GdkDevice *pointer = seat != nullptr ? gdk_seat_get_pointer(seat) : nullptr;
      double px = 0.0;
      double py = 0.0;
      if (surface != nullptr && pointer != nullptr && GDK_IS_TOPLEVEL(surface))
      {
        gdk_surface_get_device_position(surface, pointer, &px, &py, nullptr);
        gdk_toplevel_begin_move(GDK_TOPLEVEL(surface), pointer, 1, px, py, GDK_CURRENT_TIME);
      }
    }
  }
} // namespace LaVista::detail
