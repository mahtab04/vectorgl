# Building and contributing

VectorGL requires C++20 and CMake 3.21+. Visual Studio 2022/2026, GCC and
Clang are supported toolchains; VS 2026 requires CMake 4.2+. Graphics examples
need an OpenGL 3.3 driver. The default library includes GLFW, so Linux builds need GLFW's X11 development
packages. Explicit core/headless profiles avoid this dependency. GPU tests can run
with Mesa and Xvfb; ordinary library builds do not open windows or dialogs.

## Choose a build profile

Run from the repository root:

```sh
cmake --preset developer
cmake --build --preset developer
ctest --preset developer
```

| Preset | Builds |
|---|---|
| `library` | Canvas/core, UI, GLFW adapter and native dialogs; no tests, examples or apps |
| `core` | Explicit graphics-only SDK; UI disabled |
| `developer` | All library modules and CPU tests |
| `ui` | UI/host/dialog modules and CPU tests |
| `workspace` | UI, GLFW adapter, native dialogs, UI examples and CPU tests |
| `full` | All modules, examples, Flow Studio, app tests and GPU/smoke tests |
| `sanitizers` | Linux headless UI/dialog tests with ASan/UBSan |
| `quality` | Linux full source analysis using Clang/clang-tidy/clang-format 18 |

`core-release`, `library-release`, `workspace-release`, and `full-release` configure Release builds.
Generic profiles let CMake select the generator. To use Ninja with a PATH compiler:

```sh
cmake --preset workspace -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build --preset workspace
```

Each preset uses `build/<preset>`. Use a fresh directory when switching compilers
or generators. Existing `windows-debug`, `windows-release`, `linux-debug`,
`linux-release`, and `windows-mingw-*` presets remain available. The Windows
presets retain automatic Visual Studio discovery; MinGW presets use MinGW
Makefiles. Platform presets build all library modules with extras disabled; enable
tests/examples explicitly or choose a developer/workspace/full profile. `CMakeUserPresets.json` is ignored by Git for machine-specific settings.

For Visual Studio, build/test presets supply the configuration. With direct
commands, use `--config Debug` for builds and `-C Debug` for CTest. Examples stay
under `build/<preset>/example/Debug` for Visual Studio, or `example/` for Ninja.

## Build helpers

The helpers select the local compiler/generator and configure the same presets:

```bat
scripts\build.bat -Profile workspace
scripts\build.bat -Profile full -Configuration Release
scripts\build.bat -Profile library -Generator Ninja
```

Direct PowerShell invocation, if the machine restricts local scripts:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Profile workspace
```

This is a process-scoped policy setting. The default helper profile is `library`:
all library modules, with tests, examples and apps disabled. Choose `developer`
explicitly to build/run CPU tests.
`-SkipExamples` disables all example/app executables; `-SkipTests` disables all
test groups. `-Clean` removes only the selected generated build directory.
The Windows helper separates compiler/generator/profile/configuration directories
and copies MinGW runtime DLLs beside executables for Explorer launches.

```sh
./scripts/build.sh --profile workspace
./scripts/build.sh --profile full --configuration Release
CC=clang CXX=clang++ ./scripts/build.sh --profile developer --generator Ninja
```

The shell helper uses Ninja or Make and separates profile/configuration outputs.
Use a new directory or `--clean` when changing its compiler/generator.

## Direct CMake options

Fresh standalone, FetchContent and `add_subdirectory` builds provide Canvas/core,
UI, the GLFW adapter and native dialogs by default. Tests, examples, applications
and developer checks stay OFF unless explicitly enabled. Choosing a developer or
workspace preset explicitly enables its listed extras. No source-tree build or
permanent system configuration is needed.

| Option | Default | Purpose |
|---|---|---|
| `VECTORGL_BUILD_TESTS` | OFF | Engine and enabled UI tests |
| `VECTORGL_BUILD_APP_TESTS` | OFF | Flow Studio document/workspace tests |
| `VECTORGL_BUILD_EXAMPLES` | OFF | Graphical engine examples |
| `VECTORGL_BUILD_APPS` | OFF | Flow Studio executable |
| `VECTORGL_BUILD_UI` | ON | UI module; can be disabled for a graphics-only build |
| `VECTORGL_BUILD_UI_EXAMPLES` | OFF | UI examples |
| `VECTORGL_BUILD_GLFW_ADAPTER` | ON | UI host adapter; requires UI |
| `VECTORGL_BUILD_DIALOGS` | ON | Native dialog backends; requires UI |
| `VECTORGL_BUILD_GPU_TESTS` | OFF | Display-dependent tests; requires engine or app tests |
| `VECTORGL_ENABLE_HINTED_TEXT` | ON | FreeType small-text rasterization |
| `VECTORGL_ENABLE_COMPILER_WARNINGS` | ON | Private warnings on first-party targets |
| `VECTORGL_ENABLE_CLANG_TIDY` | OFF | Analysis while compiling |
| `VECTORGL_ENABLE_FORMAT_CHECK` | OFF | Full first-party formatting check |
| `VECTORGL_ENABLE_SANITIZERS` | OFF | GCC/Clang ASan and UBSan |

The old `ENABLE_CLANG_TIDY`, `ENABLE_FORMAT_CHECK`, `ENABLE_COMPILER_WARNINGS`
and `ENABLE_SANITIZERS` inputs seed the corresponding canonical option on a fresh
standalone cache. Embedded builds ignore generic parent options; use `VECTORGL_*`
there. Explicit `VECTORGL_*` values take precedence. After migration, change the
canonical option; changing an old alias does not override an existing canonical cache.
Enabling a missing analysis tool fails with a useful configure-time error.

Private warnings and analysis do not propagate to consumer targets. Sanitizer
link flags intentionally propagate so executables link the required runtime.
First-party executables are instrumented too. Sanitizers are not enabled for the
ordinary installed SDK.

## Dependencies and offline builds

GLAD is vendored with its license; no loader generation tools are required.
stb, libtess2, bundled FreeType and missing GLFW are fetched at pinned revisions.
GLFW is resolved for the default adapter, or a requested app, graphical example
or GPU test. The explicit `core` and `ci-headless` profiles disable the adapter.
VectorGL does not forcibly overwrite a parent project's GLFW cache preferences.
The bundled adapter SDK requires `GLFW_INSTALL=ON`; alternatively supply an
installed `glfw3` package, which must also be available to SDK consumers.

`VECTORGL_FREETYPE_PROVIDER` accepts `BUNDLED` (default, reproducible static SDK),
`SYSTEM` (require installed FreeType), or `AUTO` (installed first, otherwise bundled).
An existing parent-provided `Freetype::Freetype` target is reused.
System-provider SDKs require compatible FreeType on the consumer's system.
`VECTORGL_ENABLE_HINTED_TEXT=OFF` omits this dependency.

Set `VECTORGL_FETCH_DEPENDENCIES=OFF` to prohibit downloads. Supply source directories
using `FETCHCONTENT_SOURCE_DIR_STB`, `FETCHCONTENT_SOURCE_DIR_VECTORGL_LIBTESS2`,
`FETCHCONTENT_SOURCE_DIR_VECTORGL_FREETYPE`, and, when needed,
`FETCHCONTENT_SOURCE_DIR_GLFW`. `VECTORGL_STB_INCLUDE_DIR` can instead point to
installed stb headers. Missing dependencies produce a configure error, not a
silent network attempt. A first build normally needs network access unless these
inputs or installed dependencies are provided.

## Testing and quality

```sh
ctest --test-dir build/full -C Debug -L unit --output-on-failure
ctest --test-dir build/full -C Debug -L gpu --output-on-failure
ctest --test-dir build/full -C Debug -L ui --output-on-failure
cmake --build build/developer --target vectorgl-format-check
cmake --build build/developer --target vectorgl-format
```

Install clang-format 18, or pass `-DCLANG_FORMAT_EXE=/path/to/clang-format-18`,
to expose format targets. Formatting covers first-party headers, implementation,
examples, app and tests independently of enabled targets; third-party/generated
code is excluded. CI installs version 18 explicitly. Apply formatting before
submitting a change. `quality` also requires Linux host development packages
because it compiles the adapter/examples; it does not run graphical tests.

Labels are `unit`, `engine`, `ui`, `gpu`, `smoke`, `app`, and `package`. GPU tests
use `xvfb-run` when available on Linux. Native-dialog protocol tests use a fake
helper, and workspace automation injects dialog/clipboard providers. Real desktop
X11/Wayland/Windows dialog behavior still needs interactive verification.

## Installation and SDK archives

```sh
cmake --preset workspace-release
cmake --build --preset workspace-release
cmake --install build/workspace-release --config Release --prefix staging
```

Installed consumers use `find_package(vectorgl CONFIG REQUIRED)` for the core,
or request `COMPONENTS ui`, `glfw`, or `dialogs`. Link their matching namespaced
targets; UI/dialog discovery does not import GLFW. A static SDK requires compatible
compiler ABI, architecture, configuration and C++ runtime. `share/vectorgl/sdk-info.txt`
records the build details. Use an empty install prefix when changing SDK flavor;
installing core over an older full SDK does not remove stale optional files.

`scripts/package.cmake` creates a core/full archive and SHA256 checksum, extracts
it, makes the old staging prefix unavailable and tests external consumers:

```sh
cmake -DBUILD_DIR=/absolute/build/workspace-release -DOUTPUT_DIR=/absolute/release-assets \
  -DGENERATOR=Ninja -DPLATFORM=linux-x64-gcc -DFLAVOR=full \
  -P scripts/package.cmake
```

Use the build's actual generator (quote Visual Studio names). Release tags must
match `v<project version>`. Core archives contain the renderer/text/SVG SDK; full
archives additionally contain UI, GLFW adapter, native dialogs and bundled GLFW.
Examples/apps are source-build targets, not prebuilt executables in these SDKs.
Licenses accompany bundled dependencies and VectorGL itself.

## CI and contribution workflow

CI uses the same presets through `scripts/ci-build.cmake`: Linux/Windows CPU and
installed consumers, Linux sanitizers, a Linux GPU/examples/app job, graphics-only
embedding, and pinned Clang 18 quality. Windows builds compile host examples but
avoid requiring a runner GPU. The GPU job explicitly enables regular examples
so conditional smoke tests cannot silently disappear. Test reports and diagnostic
logs are uploaded even after failure. Caches contain dependency source trees and
Linux ccache outputs, never shared CMake build trees or libraries.

Release CI builds core/full SDKs on Windows/MSVC and Linux/GCC. Publication waits
for archive relocation tests and verifies the tag version. Manual workflow dispatch
builds artifacts without publishing a release. The public website and its CI live
in [vectorgl-website](https://github.com/mahtab04/vectorgl-website). Technical
Markdown guides stay in `docs/`. This repository's Pages workflow only preserves
legacy website URLs with redirects and does not run for technical guide changes.

Create a branch from current main, update relevant tests/docs, run the appropriate
profile and package checks, and open a focused PR. Generated outputs, downloaded
dependencies and machine-specific presets must stay untracked.

## FetchContent defaults

Declare VectorGL at a pinned revision, then call `FetchContent_MakeAvailable(vectorgl)`.
All four targets are available without feature options: `vectorgl::vectorgl`
(Canvas, Scene, text and SVG), `vectorgl::ui`, `vectorgl::glfw`, and
`vectorgl::dialogs`. Link the targets the application uses; host/dialog targets
transitively link UI and core. Building them does not create a window or open an
OS dialog. Use the `core` preset, or explicitly disable `VECTORGL_BUILD_UI`,
`VECTORGL_BUILD_GLFW_ADAPTER` and `VECTORGL_BUILD_DIALOGS` together, for a
graphics-only build. Linux Zenity remains a runtime dependency for native dialog
use, not a configure-time dependency.

CMake preserves existing cache choices. In an older build directory configured
with UI OFF or tests ON, set the desired options explicitly or use a fresh directory
to obtain the new defaults.

Library/platform presets explicitly set `GLFW_INSTALL=ON` so older build directories
with a cached OFF value configure with the default adapter. For a direct CMake
command, pass `-DGLFW_INSTALL=ON` once if an older cache disabled GLFW installation.
Embedded projects retain control of their GLFW cache settings.
