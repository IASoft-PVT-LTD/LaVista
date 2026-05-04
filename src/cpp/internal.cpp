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

#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <system_error>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace LaVista
{
  auto to_file_url(const filesystem::Path &path) -> String
  {
    std::error_code ec;
    filesystem::Path abs = filesystem::fs::absolute(path, ec);
    const filesystem::Path &full = ec ? path : abs;
    const String generic = String(full.generic_string().c_str());
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
    return String("file://") + encoded;
  }

  auto read_file_utf8(const filesystem::Path &path) -> Result<String>
  {
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
      return fail("Cannot open file: %s", path.string().c_str());
    }
    String out;
    char buf[16384];
    while (in.read(buf, sizeof(buf)) || in.gcount() != 0)
    {
      out.append(StringView(buf, static_cast<usize>(in.gcount())));
    }
    return out;
  }

  auto strip_root_relative_slash_after_prefix(String &html, const char *attr_prefix) -> void
  {
    const usize prefix_len = std::strlen(attr_prefix);
    usize pos = 0;
    while ((pos = html.find(attr_prefix, pos)) != String::npos)
    {
      const usize slash_pos = pos + prefix_len;
      if (slash_pos >= html.size() || html.data()[slash_pos] != '/')
      {
        pos += 1;
        continue;
      }
      if (slash_pos + 1 < html.size() && html.data()[slash_pos + 1] == '/')
      {
        pos = slash_pos + 2;
        continue;
      }
      html = String(html.substr(0, slash_pos)) + String(html.substr(slash_pos + 1));
      pos = slash_pos;
    }
  }

  auto strip_astro_serialized_root_paths(String &html) -> void
  {
    static constexpr const char *ASTRO_STR_PROP_PREFIX_FROM = "[0,&quot;/";
    static constexpr const char *ASTRO_STR_PROP_PREFIX_TO = "[0,&quot;";
    const usize from_len = std::strlen(ASTRO_STR_PROP_PREFIX_FROM);
    const usize to_len = std::strlen(ASTRO_STR_PROP_PREFIX_TO);
    usize pos = 0;
    while ((pos = html.find(ASTRO_STR_PROP_PREFIX_FROM, pos)) != String::npos)
    {
      html = String(html.substr(0, pos)) + String(ASTRO_STR_PROP_PREFIX_TO) + String(html.substr(pos + from_len));
      pos += to_len;
    }
  }

  auto inject_base_and_rewrite_spa_root_paths(String html, const filesystem::Path &bundle_dir_abs) -> String
  {
    const String base_dir_url = to_file_url(bundle_dir_abs);
    const String base_tag = String("<base href=\"") + base_dir_url + String("/\">");

    const usize head_start = html.find("<head");
    if (head_start != String::npos)
    {
      const usize head_gt = html.find('>', head_start);
      if (head_gt != String::npos)
      {
        html = String(html.substr(0, head_gt + 1)) + base_tag + String(html.substr(head_gt + 1));
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
} // namespace LaVista

namespace LaVista
{
  auto get_displays() -> Result<Vec<DisplayInfo>>
  {
    return _internal::platform_get_displays();
  }

  auto json_escape_for_string_literal(StringView raw) -> String
  {
    String out;
    out.reserve(raw.size() + 2);
    out.push_back('"');
    for (const unsigned char c : raw)
    {
      switch (c)
      {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20U)
        {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
          out += buf;
        }
        else
        {
          out.push_back(static_cast<char>(c));
        }
      }
    }
    out.push_back('"');
    return out;
  }

  auto json_encode_array(const Vec<String> &items) -> String
  {
    String s;
    s.push_back('[');
    for (usize i = 0; i < items.size(); ++i)
    {
      if (i != 0)
      {
        s.push_back(',');
      }
      s += json_escape_for_string_literal(StringView(items[i].c_str(), items[i].size()));
    }
    s.push_back(']');
    return s;
  }

  auto json_encode_array(const Vec<i32> &items) -> String
  {
    String s;
    s.push_back('[');
    for (usize i = 0; i < items.size(); ++i)
    {
      if (i != 0)
      {
        s.push_back(',');
      }
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%d", items[i]);
      s += buf;
    }
    s.push_back(']');
    return s;
  }

  auto json_encode_array(const Vec<f32> &items) -> String
  {
    String s;
    s.push_back('[');
    for (usize i = 0; i < items.size(); ++i)
    {
      if (i != 0)
      {
        s.push_back(',');
      }
      const f32 v = items[i];
      if (!std::isfinite(static_cast<double>(v)))
      {
        s += "null";
      }
      else
      {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(v));
        s += buf;
      }
    }
    s.push_back(']');
    return s;
  }
} // namespace LaVista

namespace LaVista::_internal
{
  namespace
  {
    static constexpr const char *DEFAULT_TITLEBAR_THEME = "glass";

    /* Three-column grid centers title between icon and controls with equal side tracks. */
    static constexpr char DEFAULT_TITLEBAR_HTML_TEMPLATE[] = R"LV(
<style>
:root{color-scheme:dark;}
.lavista-tb-root{--tb-bg:linear-gradient(180deg,#1f293be8 0%,#111827f0 100%);--tb-border:#334155;--tb-fg:#e2e8f0;--tb-fg-title:#f1f5f9;--tb-btn-hover:#e2e8f01a;--tb-focus:#7dd3fc;display:grid;grid-template-columns:minmax(0,1fr) minmax(0,max-content) minmax(0,1fr);align-items:center;box-sizing:border-box;height:100%;padding:0 4px 0 6px;background:var(--tb-bg);border-bottom:1px solid var(--tb-border);box-shadow:inset 0 1px 0 #ffffff12;color:var(--tb-fg);font:13px/1.1 "Segoe UI",system-ui,sans-serif;user-select:none;}
.lavista-tb-theme-neon{--tb-bg:linear-gradient(180deg,#1b1039ed 0%,#140f2cf2 100%);--tb-border:#6366f1c2;--tb-fg:#e9ecff;--tb-fg-title:#f2f4ff;--tb-btn-hover:#c4b5fd24;--tb-focus:#a5b4fc;}
.lavista-tb-theme-minimal{--tb-bg:linear-gradient(180deg,#0f172af2 0%,#111827fa 100%);--tb-border:#64748b;--tb-fg:#dbe2ea;--tb-fg-title:#f8fafc;--tb-btn-hover:#e2e8f014;--tb-focus:#93c5fd;}
.lavista-tb-left{display:flex;align-items:center;gap:8px;min-width:0;justify-self:start;padding-left:2px;}
.lavista-tb-title{font-weight:600;letter-spacing:.01em;justify-self:center;text-align:center;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;min-width:0;max-width:min(100vw - 172px,560px);padding:0 8px;color:var(--tb-fg-title);text-shadow:0 1px 1px #00000042;}
.lavista-tb-nav{display:flex;height:100%;align-items:stretch;justify-self:end;gap:2px;padding-right:2px;}
.lavista-tb-btn{-webkit-appearance:none;appearance:none;inline-size:44px;border:0;background:transparent;color:inherit;cursor:pointer;line-height:1;display:flex;align-items:center;justify-content:center;transition:background-color .13s ease,color .13s ease,transform .13s ease;border-radius:6px;padding:0;margin:2px 0;}
.lavista-tb-btn:hover{background:var(--tb-btn-hover);}
.lavista-tb-btn:active{transform:translateY(.5px);}
.lavista-tb-btn:focus-visible{outline:1px solid var(--tb-focus);outline-offset:-1px;}
.lavista-tb-close:hover{background:#dc2626;color:#fff;}
.lavista-tb-min{font-size:18px;font-weight:300;}
.lavista-tb-max svg{display:block}
</style>
<header class="lavista-tb-root <theme_class_placeholder>">
<div class="lavista-tb-left">
<svg xmlns="http://www.w3.org/2000/svg" width="22px" height="22px" viewBox="0 0 24 24" fill="none">
<path d="M20 7L4 7" stroke="#ddd" stroke-width="1.5" stroke-linecap="round"/>
<path d="M20 12L4 12" stroke="#ddd" stroke-width="1.5" stroke-linecap="round"/>
<path d="M20 17L4 17" stroke="#ddd" stroke-width="1.5" stroke-linecap="round"/>
</svg>
<window_icon_placeholder>
</div>
<span class="lavista-tb-title"><window_title_placeholder></span>
<nav class="lavista-tb-nav">
<button type="button" class="lavista-tb-btn lavista-tb-min" aria-label="Minimize" onclick="if(window.LaVista_Minimize)window.LaVista_Minimize()">&#8211;</button>
<button type="button" id="lavista-max-btn" class="lavista-tb-btn lavista-tb-max" aria-label="Maximize" onclick="if(window.LaVista_Maximize)window.LaVista_Maximize();setTimeout(function(){if(window.lavistaSyncMaxIcon)window.lavistaSyncMaxIcon();},50);setTimeout(function(){if(window.lavistaSyncMaxIcon)window.lavistaSyncMaxIcon();},220);"></button>
<button type="button" class="lavista-tb-btn lavista-tb-close" aria-label="Close" onclick="if(window.LaVista_Close)window.LaVista_Close()">
<svg width="16px" height="16px" viewBox="0 0 1024 1024" xmlns="http://www.w3.org/2000/svg"><path fill="#fff" d="M764.288 214.592 512 466.88 259.712 214.592a31.936 31.936 0 0 0-45.12 45.12L466.752 512 214.528 764.224a31.936 31.936 0 1 0 45.12 45.184L512 557.184l252.288 252.288a31.936 31.936 0 0 0 45.12-45.12L557.12 512.064l252.288-252.352a31.936 31.936 0 1 0-45.12-45.184z"/></svg>
</button>
</nav>
</header>
<script>
(function(){
var MAX_HTML="<svg width='11' height='11' viewBox='0 0 11 11' fill='none' xmlns='http://www.w3.org/2000/svg'><rect x='1.5' y='1.5' width='8' height='8' stroke='currentColor' stroke-width='1.2'/></svg>";
var RST_HTML="<svg width='11' height='11' viewBox='0 0 11 11' fill='none' xmlns='http://www.w3.org/2000/svg'><rect x='3' y='1.5' width='6.5' height='6.5' stroke='currentColor' stroke-width='1.2'/><rect x='1.5' y='3' width='6.5' height='6.5' stroke='currentColor' stroke-width='1.2'/></svg>";
function lavistaBool(v){if(v===true||v===false)return v;if(typeof v==="string"){try{return JSON.parse(v);}catch(e){return v==="true";}}return false;}
function lavistaSyncMaxIcon(){
var b=document.getElementById("lavista-max-btn");
if(!b)return;
var q=window.LaVista_QueryMaximized;
if(typeof q!=="function"){b.innerHTML=MAX_HTML;return;}
var p=q();
if(p&&typeof p.then==="function"){
p.then(function(v){var m=lavistaBool(v);b.innerHTML=m?RST_HTML:MAX_HTML;b.setAttribute("aria-label",m?"Restore":"Maximize");}).catch(function(){b.innerHTML=MAX_HTML;});
}else{
try{var isMax=lavistaBool(p);b.innerHTML=isMax?RST_HTML:MAX_HTML;b.setAttribute("aria-label",isMax?"Restore":"Maximize");}catch(e){b.innerHTML=MAX_HTML;}
}
}
window.lavistaSyncMaxIcon=lavistaSyncMaxIcon;
if(document.readyState==="loading")document.addEventListener("DOMContentLoaded",lavistaSyncMaxIcon);else lavistaSyncMaxIcon();
window.addEventListener("resize",lavistaSyncMaxIcon);
})();
</script>
)LV";

    static auto read_entire_file_binary(const filesystem::Path &path) -> Result<Vec<u8>>
    {
      std::ifstream f(path, std::ios::binary);
      if (!f)
      {
        return fail("Failed to open icon file for title bar: %s", path.string().c_str());
      }
      f.seekg(0, std::ios::end);
      const auto sz = f.tellg();
      if (sz <= 0)
      {
        return fail("Icon file for title bar is empty: %s", path.string().c_str());
      }
      Vec<u8> out(static_cast<usize>(sz));
      f.seekg(0, std::ios::beg);
      f.read(reinterpret_cast<char *>(out.data()), sz);
      if (!f)
      {
        return fail("Failed to read icon file for title bar: %s", path.string().c_str());
      }
      return out;
    }

    static auto infer_image_mime_from_path(const filesystem::Path &path) -> const char *
    {
      String ext(path.extension().string().c_str());
      for (usize i = 0; i < ext.size(); ++i)
      {
        ext.data()[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ext.data()[i])));
      }
      if (ext == ".png")
      {
        return "image/png";
      }
      if (ext == ".jpg" || ext == ".jpeg")
      {
        return "image/jpeg";
      }
      if (ext == ".gif")
      {
        return "image/gif";
      }
      if (ext == ".webp")
      {
        return "image/webp";
      }
      if (ext == ".bmp")
      {
        return "image/bmp";
      }
      if (ext == ".tga")
      {
        return "image/x-tga";
      }
      return "application/octet-stream";
    }

    static auto base64_encode(const Vec<u8> &data) -> String
    {
      static constexpr char TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      const usize len = data.size();
      String out;
      out.reserve((len + 2) / 3 * 4);
      usize i = 0;
      while (i + 3 <= len)
      {
        const u32 triple =
            (static_cast<u32>(data[i]) << 16) | (static_cast<u32>(data[i + 1]) << 8) | static_cast<u32>(data[i + 2]);
        out.push_back(TABLE[(triple >> 18) & 63]);
        out.push_back(TABLE[(triple >> 12) & 63]);
        out.push_back(TABLE[(triple >> 6) & 63]);
        out.push_back(TABLE[triple & 63]);
        i += 3;
      }
      const usize rem = len - i;
      if (rem == 1)
      {
        const u32 triple = static_cast<u32>(data[i]) << 16;
        out.push_back(TABLE[(triple >> 18) & 63]);
        out.push_back(TABLE[(triple >> 12) & 63]);
        out.push_back('=');
        out.push_back('=');
      }
      else if (rem == 2)
      {
        const u32 triple = (static_cast<u32>(data[i]) << 16) | (static_cast<u32>(data[i + 1]) << 8);
        out.push_back(TABLE[(triple >> 18) & 63]);
        out.push_back(TABLE[(triple >> 12) & 63]);
        out.push_back(TABLE[(triple >> 6) & 63]);
        out.push_back('=');
      }
      return out;
    }

    static auto html_escape_title_text(const String &title) -> String
    {
      String out;
      out.reserve(title.size() + 8);
      const char *p = title.c_str();
      for (; *p != '\0'; ++p)
      {
        const unsigned char c = static_cast<unsigned char>(*p);
        switch (c)
        {
        case '&':
          out += "&amp;";
          break;
        case '<':
          out += "&lt;";
          break;
        case '>':
          out += "&gt;";
          break;
        case '"':
          out += "&quot;";
          break;
        case '\'':
          out += "&#39;";
          break;
        default:
          out.push_back(static_cast<char>(c));
          break;
        }
      }
      return out;
    }

    static auto replace_all(String &s, const char *needle, StringView replacement) -> void
    {
      const usize nlen = std::strlen(needle);
      if (nlen == 0)
      {
        return;
      }
      usize pos = 0;
      while (true)
      {
        pos = s.find(needle, pos);
        if (pos == String::npos)
        {
          break;
        }
        s = String(s.substr(0, pos)) + String(replacement) + String(s.substr(pos + nlen));
        pos += replacement.size();
      }
    }
  } // namespace

  auto build_default_titlebar_html(const String &window_title, const String &icon_path) -> Result<String>
  {
    const filesystem::Path fs_path{icon_path.c_str()};
    auto bytes_res = read_entire_file_binary(fs_path);
    if (bytes_res.is_err())
    {
      return fail(std::move(bytes_res.unwrap_err()));
    }
    Vec<u8> bytes = std::move(bytes_res).unwrap();

    const char *const mime = infer_image_mime_from_path(fs_path);
    const String b64 = base64_encode(bytes);
    String img_tag = "<img src=\"data:";
    img_tag += mime;
    img_tag += ";base64,";
    img_tag += b64;
    img_tag += "\" width=\"22\" height=\"22\" alt=\"\" aria-hidden=\"true\" "
               "style=\"display:block;border-radius:6px;object-fit:contain;flex-shrink:0;box-shadow:0 1px 0 "
               "rgba(255,255,255,.24),0 3px 10px rgba(2,6,23,.35);\" />";

    String html(DEFAULT_TITLEBAR_HTML_TEMPLATE);
    String theme_class = "lavista-tb-theme-glass";
    if (std::strcmp(DEFAULT_TITLEBAR_THEME, "neon") == 0)
    {
      theme_class = "lavista-tb-theme-neon";
    }
    else if (std::strcmp(DEFAULT_TITLEBAR_THEME, "minimal") == 0)
    {
      theme_class = "lavista-tb-theme-minimal";
    }
    replace_all(html, "<theme_class_placeholder>", StringView(theme_class.c_str()));
    replace_all(html, "<window_icon_placeholder>", StringView(img_tag.c_str()));
    replace_all(html, "<window_title_placeholder>", StringView(html_escape_title_text(window_title).c_str()));

    return html;
  }

  auto load_inline_html_into_webview(webview_t w, String html_document) -> Result<void>
  {
    AU_TRY_VAR(temp_root, filesystem::temp_directory_path());
    filesystem::Path temp_html = std::move(temp_root);
    const u64 tick = static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());
    temp_html /=
        (String("lavista-html-") + String::format("%llu", static_cast<unsigned long long>(tick)) + ".html").c_str();
    {
      std::ofstream out(temp_html, std::ios::binary | std::ios::trunc);
      if (!out)
      {
        return fail("Cannot write temporary HTML: %s", temp_html.string().c_str());
      }
      out.write(html_document.data(), static_cast<std::streamsize>(html_document.size()));
    }

    const String file_url = to_file_url(temp_html);
    auto nav_result =
        _internal::webview_error_to_result(webview_navigate(w, file_url.c_str()), "webview_navigate(inline html)");
    if (nav_result.is_err())
    {
      std::error_code ec;
      filesystem::fs::remove(temp_html, ec);
      return fail(std::move(nav_result.unwrap_err()));
    }
    return {};
  }
} // namespace LaVista::_internal

namespace LaVista::utils
{
  auto load_spa_bundle_file_scheme(webview_t w, const filesystem::Path &index_html,
                                   const filesystem::Path &bundle_dir_abs) -> Result<void>
  {
    AU_TRY_VAR(html_raw, read_file_utf8(index_html));
    const String html = inject_base_and_rewrite_spa_root_paths(std::move(html_raw), bundle_dir_abs);

    AU_TRY_VAR(temp_root, filesystem::temp_directory_path());
    filesystem::Path temp_html = std::move(temp_root);
    const u64 tick = static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());
    temp_html /=
        (String("lavista-spa-") + String::format("%llu", static_cast<unsigned long long>(tick)) + ".html").c_str();
    {
      std::ofstream out(temp_html, std::ios::binary | std::ios::trunc);
      if (!out)
      {
        return fail("Cannot write temporary SPA HTML: %s", temp_html.string().c_str());
      }
      out.write(html.data(), static_cast<std::streamsize>(html.size()));
    }

    const String file_url = to_file_url(temp_html);
    auto nav_result =
        _internal::webview_error_to_result(webview_navigate(w, file_url.c_str()), "webview_navigate(spa bundle)");
    if (nav_result.is_err())
    {
      std::error_code ec;
      filesystem::fs::remove(temp_html, ec);
      return fail(std::move(nav_result.unwrap_err()));
    }
    return {};
  }

  auto load_image_rgba_from_file(const String &path, Vec<u8> &out_rgba, i32 &out_w, i32 &out_h) -> Result<void>
  {
    const char *const p = path.c_str();
    int w = 0;
    int h = 0;
    unsigned char *const data = stbi_load(p, &w, &h, nullptr, 4);
    if (data == nullptr)
    {
      const char *const why = stbi_failure_reason();
      return fail("Failed to decode window icon '%s': %s", p, why != nullptr ? why : "unknown error");
    }
    if (w <= 0 || h <= 0)
    {
      stbi_image_free(data);
      return fail("Decoded window icon has invalid dimensions");
    }
    const usize n = static_cast<usize>(w) * static_cast<usize>(h) * 4;
    out_rgba.resize(n);
    memcpy(out_rgba.data(), data, n);
    stbi_image_free(data);
    out_w = static_cast<i32>(w);
    out_h = static_cast<i32>(h);
    return {};
  }

  auto resize_rgba_nearest(const u8 *src, i32 sw, i32 sh, i32 dw, i32 dh, Vec<u8> &out) -> void
  {
    out.resize(static_cast<usize>(dw * dh * 4));
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0 || src == nullptr)
    {
      return;
    }
    for (i32 y = 0; y < dh; ++y)
    {
      const i32 sy = (y * sh) / dh;
      for (i32 x = 0; x < dw; ++x)
      {
        const i32 sx = (x * sw) / dw;
        const usize src_i = static_cast<usize>((sy * sw + sx) * 4);
        const usize dst_i = static_cast<usize>((y * dw + x) * 4);
        memcpy(&out[dst_i], &src[src_i], 4);
      }
    }
  }
} // namespace LaVista::utils
