# Building and Contributing to VectorGL

Thank you for helping improve VectorGL. This guide covers the supported local
build workflow, testing, formatting, and the expected contribution process.

## Prerequisites

| Tool | Minimum | Notes |
|---|---:|---|
| C++ compiler | MSVC 2022, GCC 12, or Clang 15 | Must support C++20 |
| CMake | 3.20 | Presets / VS 2022: 3.21+; VS 2026: 4.2+ |
| Git | Current stable | CMake downloads dependencies with FetchContent |
| GPU/driver | OpenGL 3.3 | Required to run graphical examples |

On Windows, install the **Desktop development with C++** workload in Visual
Studio 2022 or 2026. On Linux, install a compiler, CMake, Ninja, and the
development packages required by GLFW.

## Vendored OpenGL loader

VectorGL vendors its GLAD 2.0.8 OpenGL 3.3 Core loader under
`third_party/glad`. Normal builds do not require Python, Jinja2, or network
access for GLAD generation. See `third_party/glad/README.md` for the exact
generation settings and `third_party/glad/LICENSE` for license information.

## Recommended Windows build

The simplest option from either PowerShell or Command Prompt is:

```text
scripts\build.bat
```

The batch wrapper uses a process-scoped PowerShell execution-policy bypass; it
does not change the machine or user policy. To invoke the PowerShell script
directly on a machine that restricts local scripts:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

The helpers configure a Debug build, build the library and examples, and run
the tests. Common options are:

```powershell
# Release build
scripts\build.bat -Configuration Release

# Delete the selected build directory before configuring
scripts\build.bat -Configuration Release -Clean

# Build the library without examples or tests
scripts\build.bat -SkipExamples -SkipTests

# Override automatic CMake generator detection
scripts\build.bat -Generator Ninja
```

The Windows helper uses separate directories for each generator/compiler and
configuration, for example `build/local-visual-studio-18-2026-debug` or
`build/local-ninja-gcc-release`. Switching Visual Studio versions or moving
between MSVC and MinGW therefore does not reuse an incompatible CMake cache.
The helper prints the selected generator and output directory before building.

### Alternative Windows compilers

Visual Studio is not required. The helper supports these Windows toolchains:

| Toolchain | Automatic detection |
|---|---|
| Visual Studio with the C++ workload | Visual Studio CMake generator |
| GCC or Clang with Ninja | Ninja generator |
| MinGW-w64 with `mingw32-make` | MinGW Makefiles generator |
| MSVC developer prompt | NMake Makefiles generator |

For example, after adding an MSYS2 MinGW-w64 toolchain to `PATH`:

```bat
scripts\build.bat -Generator "MinGW Makefiles"
```

Or use GCC/Clang with Ninja:

```bat
scripts\build.bat -Generator Ninja
```

Make sure the compiler, its runtime DLLs, and the selected build tool come from
the same toolchain installation. Mixing standalone MinGW, MSYS2, and another
Ninja installation can produce compiler detection or linking failures.

After a MinGW build, the Windows helper copies the required GCC,
libstdc++, and winpthreads runtime DLLs beside every generated executable.
This allows examples and tests to be launched from Explorer without adding the
MinGW `bin` directory to the permanent system `PATH`.

## Linux and macOS build helper

Run the portable shell helper from the repository root:

```bash
./scripts/build.sh
```

Common options:

```bash
# Clean Release build
./scripts/build.sh --configuration Release --clean

# Library only
./scripts/build.sh --skip-examples --skip-tests

# Explicit compiler environment and generator
CC=clang CXX=clang++ ./scripts/build.sh --generator Ninja

```

The script chooses Ninja when available and otherwise uses Unix Makefiles. It
then configures the project, builds it, and runs CTest.

## Continuous integration

Every push and pull request runs the build and test suite in these
configurations:

- Linux GCC Debug and Release
- Linux GCC Debug with AddressSanitizer and UndefinedBehaviorSanitizer
- Windows MSVC Debug and Release
- Windows MinGW GCC with Ninja, Debug and Release
- A dedicated Linux Clang job for clang-tidy and clang-format

The MinGW jobs explicitly select `gcc`, `g++`, and Ninja, so Windows-specific
GNU compiler compatibility is tested independently from the Linux GCC jobs.
Clang-tidy and formatting are disabled in the build matrix and run once in the
dedicated quality job, avoiding duplicate diagnostics from Debug, Release, and
sanitizer builds.
Each job also installs VectorGL and builds a separate project through
`find_package(vectorgl CONFIG REQUIRED)`, verifying the exported CMake package.
The main test suite includes a hidden-window GPU integration test that loads
OpenGL through GLAD, compiles the production shaders, renders with Canvas, and
checks a read-back pixel. It runs through Mesa and Xvfb on Linux. GitHub-hosted
Windows runners do not provide a reliable desktop OpenGL 3.3 context, so the
Windows jobs run the CPU and installed-package tests but do not enable this GPU
test. You can still enable and run it on a Windows machine with a suitable
graphics driver.

The sanitizer job instruments the library with AddressSanitizer and
UndefinedBehaviorSanitizer and runs the CPU and installed-package tests. To run
the same checks locally with GCC or Clang:

```bash
cmake -S . -B build/sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SANITIZERS=ON \
  -DENABLE_CLANG_TIDY=OFF \
  -DVECTORGL_BUILD_EXAMPLES=OFF \
  -DVECTORGL_BUILD_TESTS=ON
cmake --build build/sanitize --parallel
ctest --test-dir build/sanitize --output-on-failure
```

## Manual CMake build

```bash
cmake -S . -B build/local \
  -DVECTORGL_BUILD_EXAMPLES=ON \
  -DVECTORGL_BUILD_TESTS=ON
cmake --build build/local --config Debug --parallel
ctest --test-dir build/local -C Debug --output-on-failure
```

To include the hidden-window OpenGL integration test:

```bash
cmake -S . -B build/gpu-tests \
  -DVECTORGL_BUILD_TESTS=ON \
  -DVECTORGL_BUILD_GPU_TESTS=ON
cmake --build build/gpu-tests --parallel
ctest --test-dir build/gpu-tests --output-on-failure
```

This option requires GLFW and a working OpenGL environment. On headless Linux,
install Xvfb; CMake automatically runs the GPU test through `xvfb-run` when it
is available.

For a single-configuration generator such as Ninja:

```bash
cmake -S . -B build/local -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DVECTORGL_BUILD_EXAMPLES=ON \
  -DVECTORGL_BUILD_TESTS=ON
cmake --build build/local --parallel
ctest --test-dir build/local --output-on-failure
```

## CMake presets

```bash
cmake --preset windows-debug
cmake --build --preset windows-debug

cmake --preset windows-release
cmake --build --preset windows-release
```

Linux users can replace `windows` with `linux`.

The Windows presets use CMake's default generator discovery, so the same preset
works with VS 2022 or VS 2026. Use CMake 4.2+ for VS 2026 and CMake 3.21+ for
VS 2022. You can select a particular version explicitly:

```powershell
scripts\build.bat -Generator "Visual Studio 17 2022"
scripts\build.bat -Generator "Visual Studio 18 2026"
```

For MinGW-w64, put `gcc`, `g++`, and `mingw32-make` in PATH, then run:

```powershell
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug
ctest --preset windows-mingw-debug
```

Use the Windows helper for automatic fallback to MinGW or Ninja when Visual
Studio is unavailable. Presets select CMake's default generator or an explicit
MinGW generator; they do not execute the helper's compiler detection.

If an existing preset build directory was configured with another generator,
configure into a new directory (for example `cmake --preset windows-debug -B
build/windows-vs2026`), then build that directory directly with
`cmake --build build/windows-vs2026 --config Debug`. CMake cannot switch the
generator of an existing build directory. The helper avoids this by using
separate directories automatically.

## Formatting and static analysis

When `clang-format` is installed, a formatting compliance target is added to
the normal build. Format the project with:

```bash
cmake --build build/local --target vectorgl-format
```

When `clang-tidy` is installed, it runs during compilation. Tooling can be
disabled temporarily while diagnosing a compiler-specific issue:

```bash
cmake -S . -B build/local \
  -DENABLE_CLANG_TIDY=OFF \
  -DENABLE_FORMAT_CHECK=OFF
```

Do not disable these checks merely to submit unformatted or warning-producing
code.

The checked-in `.clang-tidy` policy intentionally focuses CI on correctness,
static-analyzer, portability, and performance findings. Broad opinionated style
families are not enabled because VectorGL's public graphics API naturally uses
short coordinate and color names, public value types, and established enum
naming that those generic rules would report repeatedly.

Recommended compiler warnings are enabled independently of clang-tidy:
`/W4 /permissive-` for MSVC and `-Wall -Wextra -Wpedantic` for GCC and Clang.
They apply only while compiling VectorGL C++ sources, not vendored GLAD C code
or separately built dependencies. They can be disabled for toolchain diagnosis
with `-DENABLE_COMPILER_WARNINGS=OFF`.

## Troubleshooting

### Dependency downloads fail

GLFW and stb are fetched during initial configuration. GLAD is vendored and
does not require a download. Verify that Git can access GitHub and that a proxy
or firewall is not blocking CMake.

### Windows uses the wrong build configuration

Visual Studio is a multi-configuration generator. Include `--config Debug` or
`--config Release` when building, and `-C Debug` or `-C Release` when running
CTest. The supplied scripts always include these arguments.

The helper automatically selects an installed Visual Studio C++ workload or a
Ninja toolchain. If necessary, override it with `-Generator`, for example
`scripts\build.bat -Generator Ninja`.

## Contribution workflow

1. Fork the repository and create a focused branch.
2. Add or update tests for behavior changes.
3. Run a clean Debug and Release build when practical.
4. Keep public API changes documented in the guides or API reference.
5. Commit logically related changes together.
6. Open a pull request describing the problem, solution, and validation.

Do not commit generated build directories, IDE state, or downloaded
dependencies. These paths are already covered by `.gitignore`.
