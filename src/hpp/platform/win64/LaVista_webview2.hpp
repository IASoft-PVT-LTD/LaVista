// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

#pragma once

#include <LaVista_internal.hpp>

namespace LaVista::_internal
{
  inline constexpr wchar_t SPA_VIRTUAL_HOST_NAME_W[] = L"lavista.bundle.invalid";
  inline constexpr char SPA_VIRTUAL_HOST_NAME[] = "lavista.bundle.invalid";

  auto apply_webview2_default_background(webview_t w) -> void;
  auto map_webview2_spa_virtual_host(webview_t w, const filesystem::Path &bundle_dir_abs) -> Result<void>;
} // namespace LaVista::_internal
