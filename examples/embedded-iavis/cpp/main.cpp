// LaVista: A Modern Platform for C++ Desktop Apps.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the PolyForm Noncommercial License 1.0.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://polyformproject.org/licenses/noncommercial/1.0.0>.

#include <auxid/macros.hpp>

#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#if defined(_WIN32)
#  include <windows.h>
#endif

import lavista;
import iavis;

using namespace au;

auto run_app() -> Result<void>
{
  auto &logger = auxid::get_thread_logger();

  LaVista::WindowCreateOptions win_opts;
  win_opts.title = "Embedded IAVis Example";
  win_opts.spa_bundle_path = "examples/embedded-iavis/ui/dist";
  win_opts.icon_path = "examples/embedded-iavis/ui/public/logo-mark.png";
  win_opts.width = 1000;
  win_opts.height = 900;

  AU_TRY_VAR(window, LaVista::create_window(win_opts));

  iavis::ContextCreateInfo init_info{
      .debug_mode = true,
      .surface_width = 800,
      .surface_height = 600,
      .surface_creation_callback = nullptr,
      .surface_creation_callback_user_data = nullptr,
  };

  AU_TRY_VAR(instance, iavis::create_context(init_info));
  AU_TRY_VAR(scene, iavis::create_scene(instance));

  iavis::set_background_color(instance, {0.1f, 0.1f, 0.1f});

  iavis::Texture texture = iavis::get_default_texture(scene);
  AU_TRY_VAR(material, iavis::create_material(scene, texture, {1.0f, 1.0f, 1.0f}));

  iavis::set_projection_matrix(instance, glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f));
  iavis::set_view_matrix(
      instance, glm::lookAt(glm::vec3(2.0f, 2.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));

  const auto cube = iavis::get_cube_geometry(scene);
  AU_TRY_VAR(entity, iavis::add_entity(scene, cube, material));

  iavis::set_light_count(instance, 1);
  iavis::set_light(instance, 0, glm::vec3(2.0f, 3.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.1f),
                   glm::vec3(1.0f), glm::vec3(1.0f));

  float angle = 0.0f;
  u32 width = 800, height = 600;
  au::Vec<u8> pixels;
  pixels.resize(width * height * 4);

  while (LaVista::update_window(window))
  {
    angle += 0.05f;
    iavis::set_entity_transform(scene, entity, glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.5f, 1.0f, 0.0f)));

    iavis::render_scene(scene);

    // ignore error just in case
    (void) iavis::copy_backbuffer_to_cpu(instance, pixels);

    // invert Y axis and convert BGRA to RGBA if needed
    for (size_t i = 0; i < pixels.size(); i += 4)
    {
      std::swap(pixels[i], pixels[i + 2]);
      pixels[i + 3] = 255;
    }

    (void) LaVista::post_binary_data(window, pixels);
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
    logger.error("Application failed to run with error: {}", result.unwrap_err());
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
