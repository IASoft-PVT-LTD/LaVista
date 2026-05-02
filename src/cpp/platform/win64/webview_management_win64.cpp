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

#include <windows.h>
#include <objbase.h>

#if defined(__clang__)
#  ifndef interface
#    define interface struct
#  endif
#endif

#include <LaVista_webview2.hpp>
#include <WebView2.h>

namespace LaVista::detail
{
  auto apply_webview2_default_background(webview_t w) -> void
  {
    if (w == nullptr)
    {
      return;
    }
    void *const raw = webview_get_native_handle(w, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER);
    if (raw == nullptr)
    {
      return;
    }
    auto *const controller = static_cast<ICoreWebView2Controller *>(raw);
    ICoreWebView2Controller2 *controller2 = nullptr;
    if (FAILED(controller->QueryInterface(IID_ICoreWebView2Controller2, reinterpret_cast<void **>(&controller2))) ||
        controller2 == nullptr)
    {
      return;
    }
    COREWEBVIEW2_COLOR bg{};
    bg.A = 255;
    bg.R = 0x47;
    bg.G = 0x55;
    bg.B = 0x69;
    (void) controller2->put_DefaultBackgroundColor(bg);
    controller2->Release();
  }

  auto map_webview2_spa_virtual_host(webview_t w, const std::filesystem::path &bundle_dir_abs) -> Result<void>
  {
    if (w == nullptr)
    {
      return au::fail("WebView is null");
    }
    void *const raw = webview_get_native_handle(w, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER);
    if (raw == nullptr)
    {
      return au::fail("WebView2 controller handle unavailable");
    }
    auto *const controller = static_cast<ICoreWebView2Controller *>(raw);
    ICoreWebView2 *core = nullptr;
    if (FAILED(controller->get_CoreWebView2(&core)) || core == nullptr)
    {
      return au::fail("get_CoreWebView2 failed");
    }
    ICoreWebView2_3 *core3 = nullptr;
    if (FAILED(core->QueryInterface(IID_ICoreWebView2_3, reinterpret_cast<void **>(&core3))) || core3 == nullptr)
    {
      core->Release();
      return au::fail("ICoreWebView2_3 is not available; update the WebView2 runtime");
    }
    const std::wstring folder = bundle_dir_abs.lexically_normal().wstring();
    if (folder.size() >= static_cast<size_t>(MAX_PATH))
    {
      core3->Release();
      core->Release();
      return au::fail("SPA bundle path is too long (MAX_PATH)");
    }
    const HRESULT hr = core3->SetVirtualHostNameToFolderMapping(SPA_VIRTUAL_HOST_NAME_W, folder.c_str(),
                                                                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY);
    core3->Release();
    core->Release();
    if (FAILED(hr))
    {
      return au::fail("SetVirtualHostNameToFolderMapping failed (HRESULT 0x%lX)", static_cast<unsigned long>(hr));
    }
    return {};
  }
} // namespace LaVista::detail
