# Building and Contributing to VectorGL

Thank you for helping improve VectorGL. This guide covers the supported local
build workflow, the Python requirement used by GLAD, testing, formatting, and
the expected contribution process.

## Prerequisites

| Tool | Minimum | Notes |
|---|---:|---|
| C++ compiler | MSVC 2022, GCC 12, or Clang 15 | Must support C++20 |
| CMake | 3.20 | CMake 3.23 or newer is recommended for presets |
| Python | 3.8 | Required by the GLAD source generator |
| Git | Current stable | CMake downloads dependencies with FetchContent |
| GPU/driver | OpenGL 3.3 | Required to run graphical examples |

On Windows, install the **Desktop development with C++** workload in Visual
Studio 2022. On Linux, install a compiler, CMake, Ninja, Python, and the
development packages required by GLFW.

## Why the build needs Python

VectorGL uses GLAD 2.0.8 to generate its OpenGL 3.3 loader at build time. GLAD
is a Python program and requires Jinja2. If Jinja2 is missing, the build fails
with:

```text
ModuleNotFoundError: No module named 'jinja2'
```

Install the dependency with the same Python interpreter CMake uses:

```bash
python -m pip install "jinja2>=3,<4"
```

If multiple Python installations are present, pass the interpreter explicitly:

```bash
cmake -S . -B build -DPython_EXECUTABLE="C:/Path/To/python.exe"
```

The supplied Windows build helpers perform both checks automatically.

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

# Do not attempt to install Jinja2
scripts\build.bat -SkipPythonDependencies

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

# Select a particular Python installation
./scripts/build.sh --python /usr/bin/python3
```

The script chooses Ninja when available and otherwise uses Unix Makefiles. It
checks Python and Jinja2 before configuring GLAD, then builds and runs CTest.

## Manual CMake build

```bash
python -m pip install "jinja2>=3,<4"
cmake -S . -B build/local \
  -DPython_EXECUTABLE=/path/to/python \
  -DVECTORGL_BUILD_EXAMPLES=ON \
  -DVECTORGL_BUILD_TESTS=ON
cmake --build build/local --config Debug --parallel
ctest --test-dir build/local -C Debug --output-on-failure
```

For a single-configuration generator such as Ninja:

```bash
cmake -S . -B build/local -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPython_EXECUTABLE=/path/to/python \
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

Linux users can replace `windows` with `linux`. Install Jinja2 before using a
preset because presets do not install Python packages.

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

### GLAD cannot import Jinja2

Confirm that pip and CMake use the same interpreter:

```bash
python -c "import sys; print(sys.executable)"
python -c "import jinja2; print(jinja2.__version__)"
```

Delete the build directory after changing Python interpreters. CMake caches the
interpreter path and GLAD generation commands.

### GLAD reports that `gl.c` does not exist

This is usually a secondary error after GLAD generation failed. Find the first
Python or Jinja2 error earlier in the log, fix it, then perform a clean build:

```powershell
scripts\build.bat -Clean
```

### Dependency downloads fail

GLFW, GLAD, and stb are fetched during initial configuration. Verify that Git
can access GitHub and that a proxy or firewall is not blocking CMake.

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
