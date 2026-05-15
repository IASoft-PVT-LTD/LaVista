// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

#include <LaVista_internal.hpp>
#include <LaVista_gtk_webview.hpp>

#include <algorithm>

#include <gtk/gtk.h>
#if defined(GDK_WINDOWING_X11)
#  include <gdk/x11/gdkx.h>
#endif

namespace LaVista::_internal
{
  auto ensure_gtk_initialized() -> Result<void>
  {
    static bool initialized = false;
    if (!initialized)
    {
      if (!gtk_init_check())
      {
        return fail("gtk_init_check() failed");
      }
      initialized = true;
    }
    return {};
  }

  namespace
  {
    struct LinuxMonitorRaw
    {
      GdkRectangle rect{};
      bool primary = false;
    };

    static auto linux_collect_displays() -> Result<Vec<DisplayInfo>>
    {
      auto gtk_init_result = ensure_gtk_initialized();
      if (gtk_init_result.is_err())
      {
        return fail(std::move(gtk_init_result.unwrap_err()));
      }

      GdkDisplay *const display = gdk_display_get_default();
      if (display == nullptr)
      {
        return fail("gdk_display_get_default() returned null");
      }

      GListModel *const monitors = gdk_display_get_monitors(display);
      if (monitors == nullptr)
      {
        return fail("gdk_display_get_monitors() returned null");
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

  auto platform_get_displays() -> Result<Vec<DisplayInfo>>
  {
    return linux_collect_displays();
  }
} // namespace LaVista::_internal
