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
