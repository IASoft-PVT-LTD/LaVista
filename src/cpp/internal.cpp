// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

/**
 * @file internal.cpp
 * @brief Display query and JSON helper implementations for the `lavista` module.
 */

module;

#include <auxid/macros.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>

module lavista;

import lavista.internal;

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
