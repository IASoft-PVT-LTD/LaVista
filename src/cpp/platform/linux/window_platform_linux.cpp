// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

module;

#include <gdk/gdk.h>
#include <gdk/gdkmemorytexture.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include <cstring>
#include <string>

module lavista.internal;

#if defined(GDK_WINDOWING_X11)
#  include <X11/Xlib.h>
#  include <gdk/x11/gdkx.h>
#endif

namespace LaVista::_internal
{
  namespace
  {
    static auto linux_gtk_window_move(GtkWindow *window, int x, int y) -> void
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

    static auto linux_memory_texture_from_rgba(const Vec<u8> &buf, i32 w, i32 h) -> GdkTexture *
    {
      const gsize n = static_cast<gsize>(w) * static_cast<gsize>(h) * 4;
      if (buf.size() < n)
      {
        return nullptr;
      }
      gpointer const copy = g_malloc(n);
      memcpy(copy, buf.data(), n);
      GBytes *const bytes = g_bytes_new_take(copy, n);
      GdkTexture *const texture = gdk_memory_texture_new(static_cast<int>(w), static_cast<int>(h), GDK_MEMORY_R8G8B8A8,
                                                         bytes, static_cast<gsize>(w) * 4);
      g_bytes_unref(bytes);
      return texture;
    }

    static auto linux_apply_window_icon(GtkWindow *gtk_win, const String &icon_path) -> Result<void>
    {
      Vec<u8> rgba_full;
      i32 iw = 0;
      i32 ih = 0;
      auto load = utils::load_image_rgba_from_file(icon_path, rgba_full, iw, ih);
      if (load.is_err())
      {
        return fail(std::move(load.unwrap_err()));
      }

      Vec<u8> rgba32;
      Vec<u8> rgba16;
      utils::resize_rgba_nearest(rgba_full.data(), iw, ih, 32, 32, rgba32);
      utils::resize_rgba_nearest(rgba_full.data(), iw, ih, 16, 16, rgba16);

      GdkTexture *const t32 = linux_memory_texture_from_rgba(rgba32, 32, 32);
      GdkTexture *const t16 = linux_memory_texture_from_rgba(rgba16, 16, 16);
      if (t32 == nullptr || t16 == nullptr)
      {
        if (t32 != nullptr)
        {
          g_object_unref(t32);
        }
        if (t16 != nullptr)
        {
          g_object_unref(t16);
        }
        return fail("gdk_memory_texture_new failed for window icon");
      }

      GtkNative *native = GTK_NATIVE(gtk_win);
      GdkSurface *surface = gtk_native_get_surface(native);
      if (surface == nullptr)
      {
        gtk_widget_realize(GTK_WIDGET(gtk_win));
        surface = gtk_native_get_surface(native);
      }
      if (surface == nullptr || !GDK_IS_TOPLEVEL(surface))
      {
        g_object_unref(t32);
        g_object_unref(t16);
        return fail("Could not obtain GdkToplevel for window icon");
      }

      GList *list = nullptr;
      list = g_list_append(list, t32);
      list = g_list_append(list, t16);
      gdk_toplevel_set_icon_list(GDK_TOPLEVEL(surface), list);
      g_list_free(list);
      g_object_unref(t32);
      g_object_unref(t16);
      return {};
    }
  } // namespace

  auto platform_apply_post_webview_setup(Window_T &state, i32 width, i32 height) -> Result<void>
  {
    /* Upstream webview.h (GTK backend) has a bug where set_size_impl falls through to
     * `return error_info{WEBVIEW_ERROR_INVALID_ARGUMENT, "Invalid hint"};` unconditionally after its if/else chain
     * instead of using an else branch for the unknown-hint case, so every call to webview_set_size() on Linux
     * reports WEBVIEW_ERROR_INVALID_ARGUMENT regardless of hint and prevented the app from starting up. We own
     * the GtkWindow passed into webview_create(), so we size it directly via the GTK API here, which is exactly
     * what webview_set_size() would have done on success. */
    if (state.platform.gtk_window != nullptr)
    {
      const int w = static_cast<int>(width);
      const int h = static_cast<int>(height);
      gtk_window_set_default_size(state.platform.gtk_window, w, h);
      gtk_widget_set_size_request(GTK_WIDGET(state.platform.gtk_window), w, h);
    }
    platform_layout_webviews(static_cast<Window>(&state));
    return {};
  }

  auto platform_create_window(Window_T &state, i32 width, i32 height, i32 window_x, i32 window_y, const String &title,
                              const String &icon_path) -> Result<void>
  {
    auto gtk_init_result = ensure_gtk_initialized();
    if (gtk_init_result.is_err())
    {
      return fail(std::move(gtk_init_result.unwrap_err()));
    }

    GtkWidget *window_widget = gtk_window_new();
    gtk_window_set_default_size(GTK_WINDOW(window_widget), width, height);
    gtk_window_set_title(GTK_WINDOW(window_widget), title.c_str());
    gtk_window_set_decorated(GTK_WINDOW(window_widget), FALSE);

    state.platform.gtk_window = GTK_WINDOW(window_widget);

    /* webview_create() on GTK does `gtk_window_set_child(parent, webkit_widget)`, i.e. it overwrites the
     * window's single child slot with its widget. Pass the real window so the widget materialises, then
     * re-parent that widget into a vertical GtkBox so a future title-bar webview can share the window. Using
     * one top-level GtkWindow (rather than a second transient window positioned via X11) is the only approach
     * that works on Wayland, where absolute cross-window positioning is deliberately forbidden. */
    state.webview = webview_create(0, window_widget);
    if (state.webview == nullptr)
    {
      gtk_window_destroy(GTK_WINDOW(window_widget));
      state.platform.gtk_window = nullptr;
      return fail("webview_create() failed");
    }

    GtkWidget *const content_widget =
        static_cast<GtkWidget *>(webview_get_native_handle(state.webview, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
    if (content_widget == nullptr)
    {
      (void) webview_destroy(state.webview);
      state.webview = nullptr;
      gtk_window_destroy(GTK_WINDOW(window_widget));
      state.platform.gtk_window = nullptr;
      return fail("webview UI widget is null");
    }

    WebKitWebContext *context = webkit_web_view_get_context(WEBKIT_WEB_VIEW(content_widget));
    webkit_web_context_register_uri_scheme(
        context, "lavista-bin",
        [](WebKitURISchemeRequest *request, gpointer user_data) {
          auto *window_state = static_cast<Window>(user_data);
          const char *path = webkit_uri_scheme_request_get_path(request);
          String id = path;
          if (!id.empty() && id.data()[0] == '/')
          {
            id.assign(id.substr(1));
          }
          Vec<u8> *const buf = window_state->pending_binary_buffers.find(id);
          if (buf != nullptr)
          {
            GInputStream *stream = g_memory_input_stream_new_from_data(buf->data(), buf->size(), nullptr);
            webkit_uri_scheme_request_finish(request, stream, static_cast<gint64>(buf->size()),
                                             "application/octet-stream");
            g_object_unref(stream);
            (void) window_state->pending_binary_buffers.erase(id);
          }
          else
          {
            GError *error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_NOENT, "Buffer not found");
            webkit_uri_scheme_request_finish_error(request, error);
            g_error_free(error);
          }
        },
        &state, nullptr);

    /* Lift the widget out of window_widget's child slot before installing our box, otherwise
     * gtk_window_set_child(window_widget, box) would unparent (and potentially destroy) it. */
    g_object_ref(content_widget);
    gtk_window_set_child(GTK_WINDOW(window_widget), nullptr);

    GtkWidget *const box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_vexpand(box, TRUE);
    gtk_window_set_child(GTK_WINDOW(window_widget), box);
    state.platform.content_box = box;

    gtk_widget_set_hexpand(content_widget, TRUE);
    gtk_widget_set_vexpand(content_widget, TRUE);
    gtk_box_append(GTK_BOX(box), content_widget);
    g_object_unref(content_widget);

    gtk_widget_set_visible(window_widget, TRUE);

    {
      auto icon_result = linux_apply_window_icon(GTK_WINDOW(window_widget), icon_path);
      if (icon_result.is_err())
      {
        (void) webview_destroy(state.webview);
        state.webview = nullptr;
        state.platform.content_box = nullptr;
        gtk_window_destroy(GTK_WINDOW(window_widget));
        state.platform.gtk_window = nullptr;
        return fail(std::move(icon_result.unwrap_err()));
      }
    }

    g_signal_connect(G_OBJECT(window_widget), "destroy", G_CALLBACK(+[](GtkWidget *, gpointer user_data) {
                       auto *const window_state = reinterpret_cast<Window>(user_data);
                       if (window_state != nullptr)
                       {
                         window_state->running = false;
                         if (window_state->titlebar_webview != nullptr)
                         {
                           webview_terminate(window_state->titlebar_webview);
                         }
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

  auto platform_destroy_native(Window window) -> void
  {
    if (window == nullptr)
    {
      return;
    }
    if (window_ptr(window)->platform.titlebar_placeholder != nullptr)
    {
      gtk_window_destroy(window_ptr(window)->platform.titlebar_placeholder);
      window_ptr(window)->platform.titlebar_placeholder = nullptr;
    }
    window_ptr(window)->platform.content_box = nullptr;
  }

  auto platform_pump_events(Window window) -> bool
  {
    if (window_ptr(window)->platform.gtk_window == nullptr)
    {
      return false;
    }
    g_main_context_iteration(nullptr, FALSE);
    return true;
  }

  auto platform_sync_window_frame_from_native(Window) -> void
  {
  }

  auto platform_set_window_position(Window window, i32 x, i32 y) -> Result<void>
  {
    if (window_ptr(window)->platform.gtk_window != nullptr)
    {
      linux_gtk_window_move(window_ptr(window)->platform.gtk_window, x, y);
      platform_layout_webviews(window);
    }
    return {};
  }

  auto platform_set_window_size(Window window, i32 width, i32 height) -> Result<void>
  {
    if (window_ptr(window)->platform.gtk_window == nullptr)
    {
      return fail("Native window is null");
    }
    {
      const int w = static_cast<int>(width);
      const int h = static_cast<int>(height);
      gtk_window_set_default_size(window_ptr(window)->platform.gtk_window, w, h);
      gtk_widget_set_size_request(GTK_WIDGET(window_ptr(window)->platform.gtk_window), w, h);
      gtk_widget_queue_allocate(GTK_WIDGET(window_ptr(window)->platform.gtk_window));
    }
    platform_layout_webviews(window);
    return {};
  }

  auto platform_start_window_drag(Window window) -> void
  {
    if (window_ptr(window)->platform.gtk_window != nullptr)
    {
      GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(window_ptr(window)->platform.gtk_window));
      GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(window_ptr(window)->platform.gtk_window));
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

  auto platform_minimize_window(Window window) -> void
  {
    if (window_ptr(window)->platform.gtk_window == nullptr)
    {
      return;
    }
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(window_ptr(window)->platform.gtk_window));
    if (surface != nullptr && GDK_IS_TOPLEVEL(surface))
    {
      gdk_toplevel_minimize(GDK_TOPLEVEL(surface));
    }
  }

  auto platform_toggle_maximize_window(Window window) -> void
  {
    if (window_ptr(window)->platform.gtk_window == nullptr)
    {
      return;
    }
    if (gtk_window_is_maximized(window_ptr(window)->platform.gtk_window))
    {
      gtk_window_unmaximize(window_ptr(window)->platform.gtk_window);
    }
    else
    {
      gtk_window_maximize(window_ptr(window)->platform.gtk_window);
    }
  }

  auto platform_window_is_maximized(Window window) -> bool
  {
    if (window_ptr(window)->platform.gtk_window == nullptr)
    {
      return false;
    }
    return gtk_window_is_maximized(window_ptr(window)->platform.gtk_window) != FALSE;
  }

  auto platform_close_window(Window window) -> void
  {
    if (window_ptr(window)->platform.gtk_window != nullptr)
    {
      gtk_window_close(window_ptr(window)->platform.gtk_window);
    }
  }

  auto platform_create_titlebar_webview(Window window) -> Result<void>
  {
    if (window_ptr(window)->titlebar_webview != nullptr)
    {
      return {};
    }
    if (window_ptr(window)->platform.gtk_window == nullptr || window_ptr(window)->platform.content_box == nullptr)
    {
      return fail("Native window is null");
    }

    /* Stay-alive placeholder GtkWindow used only to satisfy webview_create()'s GtkWindow-parent requirement. It
     * is never mapped (no gtk_widget_set_visible(TRUE) call). After webview_create() installs the webkit widget
     * as this placeholder's child we steal it into `content_box` alongside the main content webview. */
    GtkWidget *const placeholder = gtk_window_new();
    window_ptr(window)->platform.titlebar_placeholder = GTK_WINDOW(placeholder);

    window_ptr(window)->titlebar_webview = webview_create(0, placeholder);
    if (window_ptr(window)->titlebar_webview == nullptr)
    {
      gtk_window_destroy(GTK_WINDOW(placeholder));
      window_ptr(window)->platform.titlebar_placeholder = nullptr;
      return fail("titlebar webview_create() failed");
    }

    GtkWidget *const tb_widget = static_cast<GtkWidget *>(
        webview_get_native_handle(window_ptr(window)->titlebar_webview, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
    if (tb_widget == nullptr)
    {
      (void) webview_destroy(window_ptr(window)->titlebar_webview);
      window_ptr(window)->titlebar_webview = nullptr;
      gtk_window_destroy(GTK_WINDOW(placeholder));
      window_ptr(window)->platform.titlebar_placeholder = nullptr;
      return fail("titlebar webview UI widget is null");
    }

    /* Reparent the widget from the placeholder into the shared content box above the main webview. */
    g_object_ref(tb_widget);
    gtk_window_set_child(GTK_WINDOW(placeholder), nullptr);

    gtk_widget_set_hexpand(tb_widget, TRUE);
    gtk_widget_set_vexpand(tb_widget, FALSE);
    const int tb_h = window_ptr(window)->titlebar_height_px > 0 ? static_cast<int>(window_ptr(window)->titlebar_height_px) : 40;
    gtk_widget_set_size_request(tb_widget, -1, tb_h);
    gtk_box_prepend(GTK_BOX(window_ptr(window)->platform.content_box), tb_widget);
    g_object_unref(tb_widget);

    return {};
  }

  auto platform_destroy_titlebar_webview(Window window) -> Result<void>
  {
    if (window_ptr(window)->titlebar_webview == nullptr)
    {
      return {};
    }

    /* Remove the widget from our box before webview_destroy() unrefs it, otherwise the box would be left
     * holding a dangling pointer. webview.h's non-owning destructor defensively no-ops its remove_child call
     * when its `m_window` (the placeholder) no longer owns the widget. */
    if (window_ptr(window)->platform.content_box != nullptr)
    {
      GtkWidget *const tb_widget = static_cast<GtkWidget *>(
          webview_get_native_handle(window_ptr(window)->titlebar_webview, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
      if (tb_widget != nullptr)
      {
        gtk_box_remove(GTK_BOX(window_ptr(window)->platform.content_box), tb_widget);
      }
    }

    auto r = webview_error_to_result(webview_destroy(window_ptr(window)->titlebar_webview), "webview_destroy(titlebar)");
    window_ptr(window)->titlebar_webview = nullptr;

    if (window_ptr(window)->platform.titlebar_placeholder != nullptr)
    {
      gtk_window_destroy(window_ptr(window)->platform.titlebar_placeholder);
      window_ptr(window)->platform.titlebar_placeholder = nullptr;
    }
    return r;
  }

  auto platform_layout_webviews(Window window) -> void
  {
    /* GtkBox handles vertical stacking and distribution automatically; we only need to keep the titlebar
     * widget's requested height in sync with window_ptr(window)->titlebar_height_px when set_window_titlebar() changes it. */
    if (window_ptr(window)->platform.content_box == nullptr || window_ptr(window)->titlebar_webview == nullptr)
    {
      return;
    }

    GtkWidget *const tb_widget = static_cast<GtkWidget *>(
        webview_get_native_handle(window_ptr(window)->titlebar_webview, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET));
    if (tb_widget == nullptr)
    {
      return;
    }
    const int tb_h = window_ptr(window)->titlebar_height_px > 0 ? static_cast<int>(window_ptr(window)->titlebar_height_px) : 0;
    gtk_widget_set_size_request(tb_widget, -1, tb_h);
  }
} // namespace LaVista::_internal

#if defined(__linux__) && !defined(__ANDROID__)
namespace LaVista::_internal
{
  auto platform_post_binary_data(Window window, const Span<const u8> &buffer) -> Result<String>
  {
    if (window == nullptr)
    {
      return fail("Window is null");
    }

    window_ptr(window)->next_binary_buffer_id++;
    String id = std::to_string(window_ptr(window)->next_binary_buffer_id).c_str();
    Vec<u8> bytes;
    const usize n = static_cast<usize>(buffer.size());
    bytes.resize(n);
    if (n > 0)
    {
      std::memcpy(bytes.data(), buffer.data(), n);
    }
    (void) window_ptr(window)->pending_binary_buffers.insert(id, std::move(bytes));

    return id;
  }
} // namespace LaVista::_internal
#endif
