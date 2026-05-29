// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

module;

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <utility>

#include <nfd.h>

module lavista;

namespace LaVista
{
  namespace
  {
    auto trim_ascii_whitespace(StringView s) -> StringView
    {
      usize a = 0;
      usize b = s.size();
      while (a < b && std::isspace(static_cast<unsigned char>(s.data()[a])))
      {
        ++a;
      }
      while (b > a && std::isspace(static_cast<unsigned char>(s.data()[b - 1])))
      {
        --b;
      }
      return s.substr(a, b - a);
    }

    auto normalize_one_extension(StringView raw) -> String
    {
      StringView e = trim_ascii_whitespace(raw);
      if (e.size() >= 2 && e.data()[0] == '*' && e.data()[1] == '.')
      {
        e = e.substr(2);
      }
      else if (!e.empty() && e.data()[0] == '.')
      {
        e = e.substr(1);
      }
      e = trim_ascii_whitespace(e);
      if (e.empty())
      {
        return {};
      }
      return String(StringView(e.data(), e.size()));
    }

    auto build_nfd_filter_list(const Span<const char *const> &filters) -> String
    {
      String out;
      for (const char *const f : filters)
      {
        if (f == nullptr)
        {
          continue;
        }
        String row;
        const StringView view(f, strlen(f));
        usize start = 0;
        for (usize i = 0; i <= view.size(); ++i)
        {
          if (i == view.size() || view.data()[i] == ',')
          {
            const StringView part = view.substr(start, i - start);
            const String one = normalize_one_extension(part);
            if (!one.empty())
            {
              if (!row.empty())
              {
                row.push_back(',');
              }
              row += one;
            }
            start = i + 1;
          }
        }
        if (!row.empty())
        {
          if (!out.empty())
          {
            out.push_back(';');
          }
          out += row;
        }
      }
      return out;
    }

    auto nfd_error_message() -> const char *
    {
      const char *e = NFD_GetError();
      return e != nullptr ? e : "Native file dialog failed";
    }
  } // namespace

  auto open_file_dialog(const Span<const char *const> &filters, const String &default_path) -> Result<Option<String>>
  {
    const String filter_str = build_nfd_filter_list(filters);
    const nfdchar_t *filter_list = filter_str.empty() ? nullptr : filter_str.c_str();

    const nfdchar_t *default_path_c = default_path.empty() ? nullptr : default_path.c_str();

    nfdchar_t *out_path = nullptr;
    const nfdresult_t result = NFD_OpenDialog(filter_list, default_path_c, &out_path);
    if (result == NFD_OKAY)
    {
      String path(out_path != nullptr ? out_path : "");
      std::free(out_path);
      return Option<String>(std::move(path));
    }
    if (result == NFD_CANCEL)
    {
      return Option<String>{};
    }
    return fail("{}", nfd_error_message());
  }

  auto open_files_dialog(const Span<const char *const> &filters, const String &default_path)
      -> Result<Option<Vec<String>>>
  {
    const String filter_str = build_nfd_filter_list(filters);
    const nfdchar_t *filter_list = filter_str.empty() ? nullptr : filter_str.c_str();

    const nfdchar_t *default_path_c = nullptr;
    if (!default_path.empty())
    {
      default_path_c = default_path.c_str();
    }

    nfdpathset_t path_set{};
    const nfdresult_t result = NFD_OpenDialogMultiple(filter_list, default_path_c, &path_set);
    if (result == NFD_OKAY)
    {
      Vec<String> paths;
      const size_t n = NFD_PathSet_GetCount(&path_set);
      paths.reserve(static_cast<usize>(n));
      for (size_t i = 0; i < n; ++i)
      {
        nfdchar_t *p = NFD_PathSet_GetPath(&path_set, i);
        if (p != nullptr)
        {
          paths.push_back(String(p));
        }
      }
      NFD_PathSet_Free(&path_set);
      return Option<Vec<String>>(std::move(paths));
    }
    if (result == NFD_CANCEL)
    {
      return Option<Vec<String>>{};
    }
    return fail("{}", nfd_error_message());
  }

  auto open_folder_dialog(const String &default_path) -> Result<Option<String>>
  {
    const nfdchar_t *default_path_c = nullptr;
    if (!default_path.empty())
    {
      default_path_c = default_path.c_str();
    }

    nfdchar_t *out_path = nullptr;
    const nfdresult_t result = NFD_PickFolder(default_path_c, &out_path);
    if (result == NFD_OKAY)
    {
      String path(out_path != nullptr ? out_path : "");
      std::free(out_path);
      return Option<String>(std::move(path));
    }
    if (result == NFD_CANCEL)
    {
      return Option<String>{};
    }
    return fail("{}", nfd_error_message());
  }

  auto save_file_dialog(const Span<const char *const> &filters, const String &default_path) -> Result<Option<String>>
  {
    const String filter_str = build_nfd_filter_list(filters);
    const nfdchar_t *filter_list = filter_str.empty() ? nullptr : filter_str.c_str();

    const nfdchar_t *default_path_c = default_path.empty() ? nullptr : default_path.c_str();

    nfdchar_t *out_path = nullptr;
    const nfdresult_t result = NFD_SaveDialog(filter_list, default_path_c, &out_path);
    if (result == NFD_OKAY)
    {
      String path(out_path != nullptr ? out_path : "");
      std::free(out_path);
      return Option<String>(std::move(path));
    }
    if (result == NFD_CANCEL)
    {
      return Option<String>{};
    }
    return fail("{}", nfd_error_message());
  }

} // namespace LaVista
