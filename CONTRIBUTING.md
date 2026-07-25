# Building and Contributing to VectorGL

Thank you for helping improve VectorGL. This guide covers the supported local
build workflow, testing, formatting, and the expected contribution process.

## Prerequisites

| Tool | Minimum | Notes |
|---|---:|---|
| C++ compiler | MSVC 2022, GCC 12, or Clang 15 | Must support C++20 |
| CMake | 3.20 | CMake 3.23 or newer is recommended for presets |
| Git | Current stable | CMake downloads dependencies with FetchContent |
| GPU/driver | OpenGL 3.3 | Required to run graphical examples |

On Windows, install the **Desktop development with C++** workload in Visual
Studio 2022. On Linux, install a compiler, CMake, Ninja, and the
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

Output is placed in `build/local-debug` or `build/local-release`.

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
- Windows MSVC Debug and Release
- Windows MinGW GCC with Ninja, Debug and Release

The MinGW jobs explicitly select `gcc`, `g++`, and Ninja, so Windows-specific
GNU compiler compatibility is tested independently from the Linux GCC jobs.

## Manual CMake build

```bash
cmake -S . -B build/local \
  -DVECTORGL_BUILD_EXAMPLES=ON \
  -DVECTORGL_BUILD_TESTS=ON
cmake --build build/local --config Debug --parallel
ctest --test-dir build/local -C Debug --output-on-failure
```

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
