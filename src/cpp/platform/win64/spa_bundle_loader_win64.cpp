// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

#include <LaVista_internal.hpp>
#include <LaVista_webview2.hpp>

namespace LaVista::_internal
{
  auto load_spa_bundle_into_webview(Window window, webview_t w, const filesystem::Path &index_html,
                                    const filesystem::Path &bundle_dir_abs) -> Result<void>
  {
    (void) window;
    (void) index_html;
    auto map_result = map_webview2_spa_virtual_host(w, bundle_dir_abs);
    if (map_result.is_err())
    {
      return fail(std::move(map_result.unwrap_err()));
    }

    const String entry_name(index_html.filename().generic_string().c_str());
    const String vhost_url = String("https://") + SPA_VIRTUAL_HOST_NAME + "/" + entry_name;
    return webview_error_to_result(webview_navigate(w, vhost_url.c_str()), "webview_navigate(spa bundle)");
  }
} // namespace LaVista::_internal
