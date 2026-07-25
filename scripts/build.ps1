[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [switch]$Clean,
    [switch]$SkipTests,
    [switch]$SkipExamples,
    [switch]$SkipPythonDependencies,

    [string]$Generator = ""
)

$ErrorActionPreference = "Stop"

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "[VectorGL] $Message" -ForegroundColor Cyan
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory)]
        [string]$Command,

        [Parameter(Mandatory)]
        [string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "'$Command' exited with code $LASTEXITCODE."
    }
}

function Find-Python {
    $candidates = @(
        @{ Command = "py"; Prefix = @("-3") },
        @{ Command = "python"; Prefix = @() },
        @{ Command = "python3"; Prefix = @() }
    )

    foreach ($candidate in $candidates) {
        if (-not (Get-Command $candidate.Command -ErrorAction SilentlyContinue)) {
            continue
        }

        $previousErrorAction = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $executable = & $candidate.Command @($candidate.Prefix) -c "import sys; print(sys.executable)" 2>$null
        $pythonExitCode = $LASTEXITCODE
        $ErrorActionPreference = $previousErrorAction
        if ($pythonExitCode -eq 0 -and $executable) {
            $selectedExecutable = [string](@($executable)[-1])
            return $selectedExecutable.Trim()
        }
    }

    throw "Python 3 was not found. Install Python 3.8 or newer and add it to PATH."
}

function Test-PythonModule {
    param(
        [Parameter(Mandatory)]
        [string]$Python,

        [Parameter(Mandatory)]
        [string]$Module
    )

    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & $Python -c "import $Module" 2>$null
    $moduleExitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorAction
    return $moduleExitCode -eq 0
}

function Find-CMakeGenerator {
    if ($Generator) {
        return $Generator
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $installationVersion = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationVersion
        if ($LASTEXITCODE -eq 0 -and $installationVersion) {
            $majorVersion = [int]($installationVersion.Split(".")[0])
            if ($majorVersion -ge 18) {
                return "Visual Studio 18 2026"
            }
            if ($majorVersion -eq 17) {
                return "Visual Studio 17 2022"
            }
        }
    }

    if ((Get-Command ninja -ErrorAction SilentlyContinue) -and
        ((Get-Command clang++ -ErrorAction SilentlyContinue) -or
         (Get-Command g++ -ErrorAction SilentlyContinue) -or
         (Get-Command cl -ErrorAction SilentlyContinue))) {
        return "Ninja"
    }

    if (Get-Command cl -ErrorAction SilentlyContinue) {
        return "NMake Makefiles"
    }

    throw @"
No supported C++ build environment was found.
Install the 'Desktop development with C++' workload in Visual Studio 2022 or
newer, or install Ninja with GCC/Clang. Then reopen the terminal and retry.
"@
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$configurationName = $Configuration.ToLowerInvariant()
$buildDirectory = Join-Path $projectRoot "build/local-$configurationName"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found. Install CMake 3.20 or newer and add it to PATH."
}

$cmakeVersion = cmake --version | Select-Object -First 1
Write-Host "[VectorGL] $cmakeVersion"

$pythonExecutable = Find-Python
Write-Host "[VectorGL] Python: $pythonExecutable"

if (-not $SkipPythonDependencies) {
    if (-not (Test-PythonModule -Python $pythonExecutable -Module "jinja2")) {
        Write-Step "Installing the GLAD generator dependency (Jinja2)"
        Invoke-Checked -Command $pythonExecutable -Arguments @(
            "-m", "pip", "install", "--disable-pip-version-check", "jinja2>=3,<4"
        )
    }
}

if (-not (Test-PythonModule -Python $pythonExecutable -Module "jinja2")) {
    throw "Jinja2 is unavailable to '$pythonExecutable'. Run: `"$pythonExecutable`" -m pip install `"jinja2>=3,<4`""
}

if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
    $resolvedBuildDirectory = (Resolve-Path -LiteralPath $buildDirectory).Path
    $expectedPrefix = (Join-Path $projectRoot "build") + [IO.Path]::DirectorySeparatorChar
    if (-not $resolvedBuildDirectory.StartsWith($expectedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean a directory outside '$expectedPrefix'."
    }

    Write-Step "Cleaning $resolvedBuildDirectory"
    Remove-Item -LiteralPath $resolvedBuildDirectory -Recurse -Force
}

$buildExamples = if ($SkipExamples) { "OFF" } else { "ON" }
$buildTests = if ($SkipTests) { "OFF" } else { "ON" }
$cmakeGenerator = Find-CMakeGenerator
Write-Host "[VectorGL] Generator: $cmakeGenerator"

$configureArguments = @(
    "-S", $projectRoot,
    "-B", $buildDirectory,
    "-G", $cmakeGenerator,
    "-DPython_EXECUTABLE=$pythonExecutable",
    "-DVECTORGL_BUILD_EXAMPLES=$buildExamples",
    "-DVECTORGL_BUILD_TESTS=$buildTests",
    "-DCMAKE_BUILD_TYPE=$Configuration"
)

if ($cmakeGenerator.StartsWith("Visual Studio", [StringComparison]::OrdinalIgnoreCase)) {
    $configureArguments += @("-A", "x64")
}

Write-Step "Configuring $Configuration in $buildDirectory"
Invoke-Checked -Command "cmake" -Arguments $configureArguments

Write-Step "Building $Configuration"
Invoke-Checked -Command "cmake" -Arguments @(
    "--build", $buildDirectory, "--config", $Configuration, "--parallel"
)

if (-not $SkipTests) {
    Write-Step "Running tests"
    Invoke-Checked -Command "ctest" -Arguments @(
        "--test-dir", $buildDirectory, "-C", $Configuration, "--output-on-failure"
    )
}

Write-Host ""
Write-Host "[VectorGL] Build completed successfully." -ForegroundColor Green
Write-Host "[VectorGL] Output: $buildDirectory"
