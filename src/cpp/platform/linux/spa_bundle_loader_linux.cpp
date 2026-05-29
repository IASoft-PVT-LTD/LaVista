// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

module;

#define WEBVIEW_HEADER
#include <webview/webview.h>
#undef WEBVIEW_HEADER

module lavista.internal;

namespace LaVista::_internal
{
  auto load_spa_bundle_into_webview(Window window, webview_t w, const filesystem::Path &index_html,
                                    const filesystem::Path &bundle_dir_abs) -> Result<void>
  {
    return utils::load_spa_bundle_file_scheme(window, w, index_html, bundle_dir_abs);
  }
} // namespace LaVista::_internal
