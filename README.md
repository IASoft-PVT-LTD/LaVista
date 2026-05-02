<div align="center">
  <img src="logo.png" alt="LaVista" width="190" style="border-radius: 1.15rem;"/>
  <br/>

  <img src="https://img.shields.io/badge/license-apache_v2-blue.svg" alt="License"/>
  <img src="https://img.shields.io/badge/standard-C%2B%2B20-yellow.svg" alt="C++20"/>
  <img src="https://img.shields.io/badge/platform-Win64%20%7C%20Linux-purple.svg" alt="Platform"/>
  <img src="https://img.shields.io/badge/compiler-MSVC%20%7C%20Clang-green.svg" alt="Compiler"/>

  <p style="padding-top: 0.2rem;">
    <b>LaVista: A modern platform for C++ desktop apps.</b>
  </p>
</div>

LaVista is a **C++20** library that hosts single-page application (SPA) bundles in an **OS-native webview**, with windowing and display management on **Windows** and **Linux**. It is built on [LibAuxid](https://github.com/I-A-S/Auxid) (Orthodox C++ core) and uses [webview](https://github.com/webview/webview) for the embedded browser control.

## Features

- **Native webview** - WebView2 on Windows; GTK 4 and WebKitGTK on Linux (API 6.0).
- **Window API** - Create and destroy windows, set title, size, position, optional custom drag region, and bind string-based events with callbacks.
- **Displays** - Query connected displays (`get_displays`) and target a display when creating a window.
- **SPA bundles** - Load front-end assets from a bundle path supplied at window creation.
- **CMake integration** - Static library target `LaVista`, public headers under `include/LaVista/`, private implementation split by platform.

Other platforms fall back to a minimal stub implementation for development; primary support matches CI (**Windows** and **Linux x64**).

## Requirements

- **CMake** 3.28 or newer (for the provided [CMake presets](CMakePresets.json); root `CMakeLists.txt` allows 3.20 for manual configures).
- **Git** with submodules - LibAuxid is expected as a submodule at `libauxid/`.
- **C++20** toolchain - MSVC or Clang, consistent with [LibAuxid](libauxid/README.md) and your chosen preset.

### Linux (Debian Based)

Typical packages (see [CI](.github/workflows/ci.yaml)):

```bash
sudo apt-get update
sudo apt-get install -y ninja-build clang pkg-config libgtk-4-dev libwebkitgtk-6.0-dev
```

X11 development libraries are required where CMake resolves `X11` (see `src/CMakeLists.txt`).

### Windows

Use **Visual Studio 2022** (or the toolchain implied by preset `LaVista-x64-windows`) or **Ninja** with MSVC / Clang as defined in LibAuxid toolchains. CI enables the MSVC environment with [ilammy/msvc-dev-cmd](https://github.com/ilammy/msvc-dev-cmd) before configuring.

## Quick start

Clone with submodules (LibAuxid is required):

```bash
git clone --recursive https://github.com/I-A-S/LaVista.git
cd LaVista
# If you already cloned without submodules:
git submodule update --init --recursive
```

Configure and build using the same presets as CI:

**Linux (Clang, Ninja)**

```bash
cmake --preset LaVista-x64-linux
cmake --build --preset LaVista-x64-linux --config Release
```

**Windows (Visual Studio generator)**

```bash
cmake --preset LaVista-x64-windows
cmake --build --preset LaVista-x64-windows --config Release
```

Additional presets (ARM64, Ninja + MSVC/Clang, Emscripten, cross-compile) are listed in [`CMakePresets.json`](CMakePresets.json).

### Using LaVista in your project

```cmake
add_subdirectory(path/to/LaVista)  # after LibAuxid is available (submodule or FetchContent)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE LaVista)
```

In code, include the public API and use the `LaVista` namespace (see [`include/LaVista/LaVista.hpp`](include/LaVista/LaVista.hpp)):

```cpp
#include <LaVista/LaVista.hpp>

// Example: create_window(WindowCreateOptions{ ... }), update_window(...), destroy_window(...)
```

Set `LaVista_BUILD_TESTS` to `OFF` when embedding LaVista if you do not want the test suite built (default is `ON` only when LaVista is the top-level project).

## License

Copyright (C) 2026 I-A-S. Licensed under the [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0).
