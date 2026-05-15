// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

#include <LaVista/LaVista.hpp>

using namespace au;

auto run_app() -> Result<void>
{
  auto &logger = auxid::get_thread_logger();

  LaVista::WindowCreateOptions win_opts;
  win_opts.title = "Hello LaVista";
  win_opts.spa_bundle_path = "spa-template/dist";
  win_opts.icon_path = "spa-template/public/logo-mark.png";
  win_opts.width = 800;
  win_opts.height = 600;

  AU_TRY_VAR(window, LaVista::create_window(win_opts));

  while (LaVista::update_window(window))
  {
  }

  AU_TRY_DISCARD(LaVista::destroy_window(window));

  return {};
}

auto app_main(int argc, char *argv[]) -> int
{
  auxid::MainThreadGuard _main_thread_guard;

  auto &logger = auxid::get_thread_logger();
  const auto result = run_app();
  if (result.is_err())
  {
    logger.error("Application failed to run with error: %s", result.unwrap_err().c_str());
    return 1;
  }
  return 0;
}

#if defined(_WIN32)
auto WINAPI WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, LPSTR lp_cmd_line, int n_show_cmd) -> int
{
  (void) h_instance;
  (void) h_prev_instance;
  (void) lp_cmd_line;
  (void) n_show_cmd;
  return app_main(0, nullptr);
}
#else
auto main(int argc, char *argv[]) -> int
{
  (void) argc;
  (void) argv;
  return app_main(argc, argv);
}
#endif
