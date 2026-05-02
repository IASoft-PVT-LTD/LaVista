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

#pragma once

#include <LaVista_internal.hpp>

namespace LaVista::detail
{
  auto load_spa_bundle_file_scheme(webview_t w, const std::filesystem::path &index_html,
                                   const std::filesystem::path &bundle_dir_abs) -> Result<void>;
} // namespace LaVista::detail
