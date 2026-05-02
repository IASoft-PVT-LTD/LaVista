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

#include <algorithm>

#include <gtk/gtk.h>
#if defined(GDK_WINDOWING_X11)
#  include <gdk/x11/gdkx.h>
#endif

namespace LaVista::detail
{
  auto ensure_gtk_initialized() -> Result<void>
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

  auto platform_get_displays() -> Result<Vec<DisplayInfo>>
  {
    return linux_collect_displays();
  }
} // namespace LaVista::detail
