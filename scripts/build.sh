#!/usr/bin/env bash

set -Eeuo pipefail

configuration="Debug"
clean=0
skip_tests=0
skip_examples=0
skip_python_dependencies=0
generator=""
python_executable=""

usage() {
    cat <<'EOF'
Build VectorGL and optionally run its tests.

Usage:
  ./scripts/build.sh [options]

Options:
  --configuration <Debug|Release>  Build configuration (default: Debug)
  --clean                          Remove the selected build directory first
  --skip-tests                     Do not build or run tests
  --skip-examples                  Do not build example applications
  --skip-python-dependencies       Do not try to install Jinja2
  --generator <name>               Override CMake generator detection
  --python <path>                  Use a specific Python interpreter
  -h, --help                       Show this help
EOF
}

fail() {
    printf '[VectorGL] Error: %s\n' "$*" >&2
    exit 1
}

step() {
    printf '\n[VectorGL] %s\n' "$*"
}

while (($#)); do
    case "$1" in
        --configuration)
            (($# >= 2)) || fail "--configuration requires a value"
            configuration="$2"
            shift 2
            ;;
        --clean)
            clean=1
            shift
            ;;
        --skip-tests)
            skip_tests=1
            shift
            ;;
        --skip-examples)
            skip_examples=1
            shift
            ;;
        --skip-python-dependencies)
            skip_python_dependencies=1
            shift
            ;;
        --generator)
            (($# >= 2)) || fail "--generator requires a value"
            generator="$2"
            shift 2
            ;;
        --python)
            (($# >= 2)) || fail "--python requires a value"
            python_executable="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "unknown option '$1' (run with --help)"
            ;;
    esac
done

case "$configuration" in
    Debug|Release) ;;
    *) fail "configuration must be Debug or Release" ;;
esac

command -v cmake >/dev/null 2>&1 ||
    fail "CMake 3.20 or newer was not found in PATH"

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
project_root="$(cd -- "$script_dir/.." && pwd -P)"
configuration_name="$(printf '%s' "$configuration" | tr '[:upper:]' '[:lower:]')"
build_directory="$project_root/build/local-$configuration_name"

if [[ -z "$python_executable" ]]; then
    for candidate in python3 python; do
        if command -v "$candidate" >/dev/null 2>&1 &&
            "$candidate" -c 'import sys; assert sys.version_info >= (3, 8)' >/dev/null 2>&1; then
            python_executable="$("$candidate" -c 'import sys; print(sys.executable)')"
            break
        fi
    done
fi

[[ -n "$python_executable" && -x "$python_executable" ]] ||
    fail "Python 3.8 or newer was not found; pass --python /path/to/python"

printf '[VectorGL] %s\n' "$(cmake --version | head -n 1)"
printf '[VectorGL] Python: %s\n' "$python_executable"

if ! "$python_executable" -c 'import jinja2' >/dev/null 2>&1; then
    if ((skip_python_dependencies)); then
        fail "Jinja2 is missing; run '$python_executable -m pip install \"jinja2>=3,<4\"'"
    fi

    step "Installing the GLAD generator dependency (Jinja2)"
    "$python_executable" -m pip install --disable-pip-version-check 'jinja2>=3,<4'
fi

if [[ -z "$generator" ]]; then
    if command -v ninja >/dev/null 2>&1 &&
        { command -v c++ >/dev/null 2>&1 ||
          command -v g++ >/dev/null 2>&1 ||
          command -v clang++ >/dev/null 2>&1; }; then
        generator="Ninja"
    elif command -v make >/dev/null 2>&1 &&
        { command -v c++ >/dev/null 2>&1 ||
          command -v g++ >/dev/null 2>&1 ||
          command -v clang++ >/dev/null 2>&1; }; then
        generator="Unix Makefiles"
    else
        fail "no C++ toolchain was found; install Ninja or Make with GCC/Clang"
    fi
fi

printf '[VectorGL] Generator: %s\n' "$generator"

if ((clean)) && [[ -e "$build_directory" ]]; then
    case "$build_directory" in
        "$project_root"/build/local-debug|"$project_root"/build/local-release)
            step "Cleaning $build_directory"
            rm -rf -- "$build_directory"
            ;;
        *)
            fail "refusing to clean unexpected path '$build_directory'"
            ;;
    esac
fi

build_examples="ON"
build_tests="ON"
((skip_examples)) && build_examples="OFF"
((skip_tests)) && build_tests="OFF"

configure_arguments=(
    -S "$project_root"
    -B "$build_directory"
    -G "$generator"
    "-DCMAKE_BUILD_TYPE=$configuration"
    "-DPython_EXECUTABLE=$python_executable"
    "-DVECTORGL_BUILD_EXAMPLES=$build_examples"
    "-DVECTORGL_BUILD_TESTS=$build_tests"
)

step "Configuring $configuration in $build_directory"
cmake "${configure_arguments[@]}"

step "Building $configuration"
cmake --build "$build_directory" --config "$configuration" --parallel

if ((!skip_tests)); then
    step "Running tests"
    ctest --test-dir "$build_directory" -C "$configuration" --output-on-failure
fi

printf '\n[VectorGL] Build completed successfully.\n'
printf '[VectorGL] Output: %s\n' "$build_directory"
