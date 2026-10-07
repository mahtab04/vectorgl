# Exercise generator selection without requiring every compiler installation.
$ErrorActionPreference = 'Stop'
$tokens = $null
$errors = $null
$helperPath = Join-Path $PSScriptRoot '../scripts/build.ps1'
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    $helperPath, [ref]$tokens, [ref]$errors)
if ($errors) { throw ($errors | Out-String) }
$selectionFunction = $ast.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq 'Find-CMakeGenerator'
}, $true)
. ([scriptblock]::Create($selectionFunction.Extent.Text))

# Mocks are confined to this test process.
function Get-Command {
    param($Name, $ErrorAction)
    if ($Name -in $availableCommands) { [pscustomobject]@{ Source = $Name } }
}
function Join-Path { param($Path, $ChildPath) 'Invoke-MockVswhere' }
function Test-Path { param($LiteralPath) $true }
function Invoke-MockVswhere {
    $global:LASTEXITCODE = 0
    $installedVersions
}
function Expect-Generator {
    param($Expected)
    $actual = Find-CMakeGenerator
    if ($actual -ne $Expected) { throw "Expected '$Expected', got '$actual'." }
}
function Expect-Error {
    param($Message)
    $caught = $false
    try { Find-CMakeGenerator | Out-Null }
    catch {
        $caught = $true
        if ($_.Exception.Message -notlike "*$Message*") { throw }
    }
    if (-not $caught) { throw 'Expected generator selection to fail.' }
}

$env:CMAKE_GENERATOR = ''
$env:CXX = ''
${env:ProgramFiles(x86)} = 'mock-installations'
$Generator = ''
$supportedGenerators = @('Visual Studio 18 2026', 'Visual Studio 17 2022',
    'Ninja', 'NMake Makefiles', 'MinGW Makefiles')
$availableCommands = @()
$installedVersions = @('17.12.0')
Expect-Generator 'Visual Studio 17 2022'
$installedVersions = @('18.10.0')
Expect-Generator 'Visual Studio 18 2026'
$installedVersions = @('17.12.0', '18.10.0')
Expect-Generator 'Visual Studio 18 2026'
$supportedGenerators = @('Visual Studio 17 2022', 'Ninja', 'NMake Makefiles', 'MinGW Makefiles')
Expect-Generator 'Visual Studio 17 2022'
$installedVersions = @('18.10.0')
Expect-Error 'CMake 4.2+'
$installedVersions = @()
$availableCommands = @('g++', 'gcc', 'mingw32-make')
Expect-Generator 'MinGW Makefiles'
$availableCommands = @('g++', 'gcc', 'ninja')
Expect-Generator 'Ninja'
$availableCommands = @('cl')
Expect-Generator 'NMake Makefiles'
$availableCommands = @()
Expect-Error 'No supported C++'
$Generator = 'MinGW Makefiles'
Expect-Generator 'MinGW Makefiles'
$Generator = 'Visual Studio 18 2026'
Expect-Error 'does not support generator'
$Generator = ''
$env:CMAKE_GENERATOR = 'Ninja'
Expect-Generator 'Ninja'
Write-Host 'Generator selection tests passed (VS 2022, VS 2026, MinGW, Ninja, overrides, and errors).'
