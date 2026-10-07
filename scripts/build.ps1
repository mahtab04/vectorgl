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
        if ($Generator -notin $supportedGenerators) {
            throw "CMake does not support generator '$Generator'. Update CMake or choose a supported generator."
        }
        return $Generator
    }

    if ($env:CMAKE_GENERATOR) {
        if ($env:CMAKE_GENERATOR -notin $supportedGenerators) {
            throw "CMAKE_GENERATOR='$env:CMAKE_GENERATOR' is not supported by this CMake installation."
        }
        return $env:CMAKE_GENERATOR
    }

    if ((Get-Command ninja -ErrorAction SilentlyContinue) -and
        ($env:CXX -or (Get-Command clang++ -ErrorAction SilentlyContinue) -or
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

    $vswhere = if (${env:ProgramFiles(x86)}) {
        Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    }
    if ($vswhere -and (Test-Path -LiteralPath $vswhere)) {
        $installationVersions = & $vswhere -all -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationVersion
        if ($LASTEXITCODE -eq 0 -and $installationVersions) {
            foreach ($installationVersion in @($installationVersions | Sort-Object { [version]$_ } -Descending)) {
                $majorVersion = [int]($installationVersion.Split(".")[0])
                $candidate = switch ($majorVersion) {
                    18 { "Visual Studio 18 2026" }
                    17 { "Visual Studio 17 2022" }
                }
                if ($candidate -and $candidate -in $supportedGenerators) {
                    return $candidate
                }
            }
            throw "Installed Visual Studio versions are not supported by this CMake. VS 2026 needs CMake 4.2+; VS 2022 needs CMake 3.21+."
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

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found. Install CMake 3.20 or newer and add it to PATH."
}

$cmakeVersion = cmake --version | Select-Object -First 1
Write-Host "[VectorGL] $cmakeVersion"
$capabilitiesText = & cmake -E capabilities
if ($LASTEXITCODE -ne 0) {
    throw "Unable to query CMake generators."
}
$supportedGenerators = @(($capabilitiesText | Out-String | ConvertFrom-Json).generators.name)
$cmakeGenerator = Find-CMakeGenerator

# Pin PATH compilers for Make/Ninja so a second installed toolchain cannot
# silently change the compiler selected by the helper.
$compilerArguments = @()
$compilerTag = ""
if ($cmakeGenerator -eq "MinGW Makefiles") {
    if (-not (Get-Command gcc -ErrorAction SilentlyContinue) -or
        -not (Get-Command g++ -ErrorAction SilentlyContinue)) {
        throw "MinGW Makefiles requires gcc and g++ in PATH."
    }
    $compilerArguments = @("-DCMAKE_C_COMPILER=gcc", "-DCMAKE_CXX_COMPILER=g++")
    $compilerTag = "-gcc"
} elseif ($cmakeGenerator -eq "Ninja") {
    if ($env:CXX) {
        $compilerTag = "-" + ([IO.Path]::GetFileNameWithoutExtension($env:CXX) -replace '[^a-zA-Z0-9]+', '-')
    } elseif (Get-Command cl -ErrorAction SilentlyContinue) {
        $compilerArguments = @("-DCMAKE_C_COMPILER=cl", "-DCMAKE_CXX_COMPILER=cl")
        $compilerTag = "-msvc"
    } elseif ((Get-Command gcc -ErrorAction SilentlyContinue) -and (Get-Command g++ -ErrorAction SilentlyContinue)) {
        $compilerArguments = @("-DCMAKE_C_COMPILER=gcc", "-DCMAKE_CXX_COMPILER=g++")
        $compilerTag = "-gcc"
    } elseif ((Get-Command clang -ErrorAction SilentlyContinue) -and (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        $compilerArguments = @("-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++")
        $compilerTag = "-clang"
    } else {
        throw "Ninja requires an initialized MSVC developer prompt, GCC, or Clang in PATH."
    }
}
$generatorTag = ($cmakeGenerator.ToLowerInvariant() -replace '[^a-z0-9]+', '-').Trim('-')
$buildDirectory = Join-Path $projectRoot "build/local-$generatorTag$compilerTag-$configurationName"

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
Write-Host "[VectorGL] Generator: $cmakeGenerator"

$configureArguments = @(
    "-S", $projectRoot,
    "-B", $buildDirectory,
    "-G", $cmakeGenerator,
    "-DVECTORGL_BUILD_EXAMPLES=$buildExamples",
    "-DVECTORGL_BUILD_TESTS=$buildTests",
    "-DCMAKE_BUILD_TYPE=$Configuration"
)
$configureArguments += $compilerArguments

if ($cmakeGenerator.StartsWith("Visual Studio", [StringComparison]::OrdinalIgnoreCase)) {
    $configureArguments += @("-A", "x64")
}

Write-Step "Configuring $Configuration in $buildDirectory"
Invoke-Checked -Command "cmake" -Arguments $configureArguments

Write-Step "Building $Configuration"
Invoke-Checked -Command "cmake" -Arguments @(
    "--build", $buildDirectory, "--config", $Configuration, "--parallel"
)

$cachePath = Join-Path $buildDirectory "CMakeCache.txt"
$compilerEntry = Get-Content -LiteralPath $cachePath | Where-Object { $_ -match '^CMAKE_CXX_COMPILER:(FILEPATH|STRING)=' } |
    Select-Object -First 1
if ($compilerEntry) {
    $compilerPath = $compilerEntry.Substring($compilerEntry.IndexOf('=') + 1)
    if ([IO.Path]::GetFileName($compilerPath) -match '^(.*-)?g\+\+(\.exe)?$') {
        $runtimeDirectory = Split-Path -Parent $compilerPath
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
