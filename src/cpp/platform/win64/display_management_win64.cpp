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

#include <algorithm>
#include <windows.h>

namespace LaVista::detail
{
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

  auto platform_get_displays() -> Result<Vec<DisplayInfo>>
  {
    return win32_collect_displays();
  }
} // namespace LaVista::detail
