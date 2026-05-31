# LaVista

**LaVista: A modern platform for C++ desktop apps.**

LaVista is a C++23 library designed to host single-page application (SPA) bundles within an **OS-native webview**. It provides robust windowing and display management for **Windows** and **Linux**.

Under the hood, LaVista is built on [LibAuxid](https://github.com/I-A-S/Auxid) and uses [webview](https://github.com/webview/webview) for the embedded browser. Other platforms currently use a minimal stub for development; primary support and CI focus on Windows and Linux x64.

!!! note "Module-only API"
    LaVista has been migrated to a module-only API. Public entry point: `import lavista;` in namespace `LaVista`, with LibAuxid types re-exported from `au`.

## Features

- **Native Webviews** — WebView2 on Windows; GTK 4 and WebKitGTK on Linux (API 6.0).
- **Default Host Title Bar** — Native-looking chrome strip above your SPA with title and embedded icon.
- **Custom HTML Title Bar** — `set_window_titlebar` for a dedicated webview band with drag-to-move.
- **Native Window Icons** — `WindowCreateOptions.icon_path` (PNG, JPEG, or stb_image-supported formats).
- **Comprehensive Window API** — Create, destroy, resize, move, and optional drag strips.
- **Event Binding** — `bind_window_event` / `bind_window_function` connect the SPA to C++.
- **Display Management** — `get_displays` and `WindowCreateOptions.display_index`.
- **SPA Integration** — Point `spa_bundle_path` at your build output; use [liblavista](liblavista.md) in the frontend.
- **CMake** — Static `LaVista` target with C++23 modules.

See the [API Reference](lavista/links.md) for full C++ documentation.

## Requirements

- **CMake** 3.28+ (C++ modules; see `CMakePresets.json` in the repository root)
- **Git** with submodule support (LibAuxid at `libauxid/`)
- **C++23 compiler** — MSVC, Clang, or GCC 15.2+, matching [LibAuxid](https://github.com/I-A-S/Auxid)

### Linux (Debian-based)

```bash
sudo apt-get update
sudo apt-get install -y ninja-build clang pkg-config libgtk-4-dev libwebkitgtk-6.0-dev
```

X11 development libraries are required where CMake resolves `X11` (see `src/CMakeLists.txt`).

### Windows

Use **Visual Studio 2026** with preset `LaVista-x64-windows`, or **Ninja** with `LaVista-x64-windows-msvc` / `LaVista-x64-windows-clang`.

## Quick start

Clone with submodules:

```bash
git clone --recursive https://github.com/I-A-S/LaVista.git
cd LaVista

# If you cloned without --recursive:
# git submodule update --init --recursive
```

Build with presets:

**Linux (Clang, Ninja)**

```bash
cmake --preset LaVista-x64-linux
cmake --build --preset LaVista-x64-linux --config Release
```

**Windows (Visual Studio 2026)**

```bash
cmake --preset LaVista-x64-windows
cmake --build --preset LaVista-x64-windows --config Release
```

Additional ARM64 and Ninja presets are in `CMakePresets.json` at the repository root.

### Hello LaVista example

The `HelloLaVista` target loads the bundled [Astro](https://astro.build/) template in `spa-template/`.

Build the frontend:

```bash
cd spa-template
npm install
npm run build
cd ..
```

Run `HelloLaVista` from the repository root so paths like `spa-template/dist` resolve correctly. The binary is typically under `out/build/<preset>/bin/`.

### Using LaVista in your project

```cmake
# LibAuxid must be available first (submodule or FetchContent)
add_subdirectory(path/to/LaVista)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE LaVista)
set_target_properties(my_app PROPERTIES CXX_SCAN_FOR_MODULES ON)
```

```cpp
#include <auxid/macros.hpp>

import lavista;

using namespace au;

LaVista::WindowCreateOptions opts;
opts.title = "My App";
opts.spa_bundle_path = "path/to/dist";
opts.icon_path = "path/to/icon.png";
opts.width = 1024;
opts.height = 768;

AU_TRY_VAR(window, LaVista::create_window(opts));

while (LaVista::update_window(window))
{
}

AU_TRY_DISCARD(LaVista::destroy_window(window));
```

Public API: `import lavista;` (re-exports [LibAuxid](https://github.com/I-A-S/Auxid)), namespace `LaVista`. Use `<auxid/macros.hpp>` for `AU_TRY_*` helpers.

## License

Copyright © 2026 IASoft (PVT) LTD. Licensed under the [PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0).
