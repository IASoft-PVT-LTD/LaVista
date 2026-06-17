// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

#pragma once

#include <windows.h>

namespace LaVista
{
  struct PlatformWindowContext
  {
    HWND hwnd = nullptr;
    HICON icon_small = nullptr;
    HICON icon_big = nullptr;
    // Last immersive-frame state pushed to the OS: -1 = none yet, 0 = light, 1 = dark. Used to
    // force a one-time frame repaint when the value changes (Windows 10 won't otherwise refresh
    // the border until the window is resized).
    int frame_dark = -1;
  };
} // namespace LaVista
