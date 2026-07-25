[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [switch]$Clean,
    [switch]$SkipTests,
    [switch]$SkipExamples,

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

function Find-CMakeGenerator {
    if ($Generator) {
        return $Generator
    }

    if ((Get-Command ninja -ErrorAction SilentlyContinue) -and
        ((Get-Command clang++ -ErrorAction SilentlyContinue) -or
         (Get-Command g++ -ErrorAction SilentlyContinue) -or
         (Get-Command cl -ErrorAction SilentlyContinue))) {
        return "Ninja"
    }

    if ((Get-Command mingw32-make -ErrorAction SilentlyContinue) -and
        (Get-Command g++ -ErrorAction SilentlyContinue)) {
        return "MinGW Makefiles"
    }

    if (Get-Command cl -ErrorAction SilentlyContinue) {
        return "NMake Makefiles"
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

    throw @"
No supported C++ build environment was found.
Install the 'Desktop development with C++' workload in Visual Studio 2022 or
newer, install Ninja with GCC/Clang, or install MinGW-w64 with mingw32-make.
Then reopen the terminal and retry.
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

if ($cmakeGenerator -eq "MinGW Makefiles" -or
    ($cmakeGenerator -eq "Ninja" -and (Get-Command g++.exe -ErrorAction SilentlyContinue))) {
    $compiler = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($compiler) {
        $runtimeDirectory = Split-Path -Parent $compiler.Source
        $runtimeFiles = @(
            Get-ChildItem -LiteralPath $runtimeDirectory -Filter "libgcc_s_*.dll" -File
            Get-ChildItem -LiteralPath $runtimeDirectory -Filter "libstdc++-6.dll" -File
            Get-ChildItem -LiteralPath $runtimeDirectory -Filter "libwinpthread-1.dll" -File
        )
        $executableDirectories = Get-ChildItem -LiteralPath $buildDirectory -Filter "*.exe" -File -Recurse |
            Select-Object -ExpandProperty DirectoryName -Unique

        foreach ($runtimeFile in $runtimeFiles) {
            foreach ($executableDirectory in $executableDirectories) {
                Copy-Item -LiteralPath $runtimeFile.FullName -Destination $executableDirectory -Force
            }
        }

        Write-Host "[VectorGL] Deployed MinGW runtime DLLs beside built executables."
    }
}

if (-not $SkipTests) {
    Write-Step "Running tests"
    Invoke-Checked -Command "ctest" -Arguments @(
        "--test-dir", $buildDirectory, "-C", $Configuration, "--output-on-failure"
    )
}

Write-Host ""
Write-Host "[VectorGL] Build completed successfully." -ForegroundColor Green
Write-Host "[VectorGL] Output: $buildDirectory"
