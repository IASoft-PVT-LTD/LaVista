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

#include <spa_bundle.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace LaVista::detail
{
  namespace
  {
    auto to_file_url(const std::filesystem::path &path) -> String
    {
      const String generic = String(std::filesystem::absolute(path).generic_string().c_str());
      String encoded;
      encoded.reserve(generic.size() + 8);
      for (const char c : generic)
      {
        switch (c)
        {
        case ' ':
          encoded += "%20";
          break;
        case '#':
          encoded += "%23";
          break;
        case '?':
          encoded += "%3F";
          break;
        default:
          encoded.push_back(c);
          break;
        }
      }
      return String("file://") + String(encoded.c_str());
    }

    auto read_file_utf8(const std::filesystem::path &path) -> Result<std::string>
    {
      std::ifstream in(path, std::ios::binary);
      if (!in)
      {
        return au::fail("Cannot open file: %s", path.string().c_str());
      }
      std::ostringstream ss;
      ss << in.rdbuf();
      return ss.str();
    }

    auto strip_root_relative_slash_after_prefix(std::string &html, const char *attr_prefix) -> void
    {
      const size_t prefix_len = std::strlen(attr_prefix);
      size_t pos = 0;
      while ((pos = html.find(attr_prefix, pos)) != std::string::npos)
      {
        const size_t slash_pos = pos + prefix_len;
        if (slash_pos >= html.size() || html[slash_pos] != '/')
        {
          pos += 1;
          continue;
        }
        if (slash_pos + 1 < html.size() && html[slash_pos + 1] == '/')
        {
          pos = slash_pos + 2;
          continue;
        }
        html.erase(slash_pos, 1);
        pos = slash_pos;
      }
    }

    auto strip_astro_serialized_root_paths(std::string &html) -> void
    {
      static constexpr const char *ASTRO_STR_PROP_PREFIX_FROM = "[0,&quot;/";
      static constexpr const char *ASTRO_STR_PROP_PREFIX_TO = "[0,&quot;";
      const size_t from_len = std::strlen(ASTRO_STR_PROP_PREFIX_FROM);
      const size_t to_len = std::strlen(ASTRO_STR_PROP_PREFIX_TO);
      size_t pos = 0;
      while ((pos = html.find(ASTRO_STR_PROP_PREFIX_FROM, pos)) != std::string::npos)
      {
        html.replace(pos, from_len, ASTRO_STR_PROP_PREFIX_TO);
        pos += to_len;
      }
    }

    auto inject_base_and_rewrite_spa_root_paths(std::string html, const std::filesystem::path &bundle_dir_abs)
        -> std::string
    {
      const String base_dir_url = to_file_url(bundle_dir_abs);
      const std::string base_tag = std::string("<base href=\"") + base_dir_url.c_str() + std::string("/\">");

      const size_t head_start = html.find("<head");
      if (head_start != std::string::npos)
      {
        const size_t head_gt = html.find('>', head_start);
        if (head_gt != std::string::npos)
        {
          html.insert(head_gt + 1, base_tag);
        }
      }

      strip_root_relative_slash_after_prefix(html, "href=\"");
      strip_root_relative_slash_after_prefix(html, "src=\"");
      strip_root_relative_slash_after_prefix(html, "action=\"");
      strip_root_relative_slash_after_prefix(html, "srcset=\"");
      strip_root_relative_slash_after_prefix(html, "component-url=\"");
      strip_root_relative_slash_after_prefix(html, "renderer-url=\"");
      strip_root_relative_slash_after_prefix(html, "before-hydration-url=\"");
      strip_astro_serialized_root_paths(html);

      return html;
    }
  } // namespace

  auto load_spa_bundle_file_scheme(webview_t w, const std::filesystem::path &index_html,
                                   const std::filesystem::path &bundle_dir_abs) -> Result<void>
  {
    AU_TRY_VAR(html_raw, read_file_utf8(index_html));
    const std::string html = inject_base_and_rewrite_spa_root_paths(std::move(html_raw), bundle_dir_abs);

    std::filesystem::path temp_html = std::filesystem::temp_directory_path();
    temp_html /=
        std::string("lavista-spa-") +
        std::to_string(static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count())) +
        ".html";
    {
      std::ofstream out(temp_html, std::ios::binary | std::ios::trunc);
      if (!out)
      {
        return au::fail("Cannot write temporary SPA HTML: %s", temp_html.string().c_str());
      }
      out << html;
    }

    const String file_url = to_file_url(temp_html);
    auto nav_result = webview_error_to_result(webview_navigate(w, file_url.c_str()), "webview_navigate(spa bundle)");
    if (nav_result.is_err())
    {
      std::error_code ec;
      std::filesystem::remove(temp_html, ec);
      return au::fail(std::move(nav_result.unwrap_err()));
    }
    return {};
  }
} // namespace LaVista::detail
