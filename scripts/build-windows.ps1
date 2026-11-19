param(
    [ValidateSet('build', 'test', 'opengl-demo', 'pong-demo', 'clean')]
    [string]$Target = 'build',
    [string]$Cxx = 'g++'
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $Root

function Fail([string]$Message) {
    Write-Error $Message
    exit 1
}

function Invoke-Native([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        Fail "Command failed with exit code $LASTEXITCODE`: $Program $($Arguments -join ' ')"
    }
}

if ($Target -eq 'clean') {
    if (Test-Path 'build') { Remove-Item -Recurse -Force 'build' }
    if (Test-Path 'bylc.exe') { Remove-Item -Force 'bylc.exe' }
    Write-Host 'BYlink build directory and root bylc.exe removed.'
    exit 0
}

$compilerCommand = Get-Command $Cxx -ErrorAction SilentlyContinue
if (-not $compilerCommand) {
    Fail "C++ compiler '$Cxx' was not found. Install MinGW-w64 and ensure g++.exe is in PATH."
}
$Cxx = $compilerCommand.Source
New-Item -ItemType Directory -Force -Path 'build' | Out-Null

$coreSources = @(
    'compiler/common/diagnostic.cpp',
    'compiler/common/process.cpp',
    'compiler/target/target.cpp',
    'compiler/types/types.cpp',
    'compiler/lexer/lexer.cpp',
    'compiler/parser/parser.cpp',
    'compiler/semantic/sema.cpp',
    'compiler/ir/ir.cpp',
    'compiler/ir/lower.cpp',
    'compiler/ir/verify.cpp',
    'compiler/ai/byai.cpp',
    'compiler/ai/byb.cpp',
    'compiler/optimizer/optimizer.cpp',
    'compiler/codegen/llvm_emitter.cpp',
    'compiler/codegen/toolchain.cpp',
    'compiler/driver/pipeline.cpp',
    'compiler/opengl/bygl.cpp',
    'compiler/opengl/bygl2.cpp'
)

$buildArgs = @('-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Icompiler') + $coreSources + @('compiler/driver/main.cpp', '-o', 'build/bylc.exe')
Write-Host 'Building BYlink compiler directly with MinGW (no CMake)...' -ForegroundColor Cyan
Invoke-Native $Cxx $buildArgs

# Keep a fresh compiler beside the project Makefile. This avoids accidentally
# invoking an older bylc.exe copied elsewhere (for example, Downloads\bylc.exe).
Copy-Item -Force 'build/bylc.exe' 'bylc.exe'
Write-Host 'Updated root compiler: bylc.exe' -ForegroundColor Green

if ($Target -eq 'build') {
    Write-Host 'BUILD OK: build/bylc.exe and bylc.exe' -ForegroundColor Green
    exit 0
}

if ($Target -eq 'pong-demo') {
    Write-Host 'Compiling BYGL 2 interactive Pong...' -ForegroundColor Cyan
    Invoke-Native (Join-Path $Root 'build/bylc.exe') @('examples/pong.bygl', '-o', 'build/pong.exe')
    Write-Host 'Launching build/pong.exe (W/S vs Up/Down, Escape quits)...' -ForegroundColor Green
    Invoke-Native (Join-Path $Root 'build/pong.exe') @()
    exit 0
}

if ($Target -eq 'opengl-demo') {
    Write-Host 'Compiling BYGL demo to a native Windows executable...' -ForegroundColor Cyan
    Invoke-Native (Join-Path $Root 'build/bylc.exe') @('examples/rotating_scene.bygl', '-o', 'build/rotating_scene.exe')
    Write-Host 'Launching build/rotating_scene.exe (press Escape to close)...' -ForegroundColor Green
    Invoke-Native (Join-Path $Root 'build/rotating_scene.exe') @()
    exit 0
}

if ($Target -eq 'test') {
    $testSources = @(
        'tests/test_main.cpp',
        'tests/test_types.cpp',
        'tests/test_lexer.cpp',
        'tests/test_parser.cpp',
        'tests/test_semantic.cpp',
        'tests/test_ir.cpp',
        'tests/test_ai.cpp',
        'tests/test_robustness.cpp',
        'tests/test_opengl.cpp'
    )
    $exampleDir = (Join-Path $Root 'examples').Replace('\', '/')
    $testArgs = @('-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Icompiler', '-Itests', "-DBYLINK_EXAMPLES_DIR=`"$exampleDir`"") + $coreSources + $testSources + @('-o', 'build/bylink_unit_tests.exe')
    Write-Host 'Building the compiler unit tests...' -ForegroundColor Cyan
    Invoke-Native $Cxx $testArgs
    Write-Host 'Running unit tests...' -ForegroundColor Cyan
    Invoke-Native (Join-Path $Root 'build/bylink_unit_tests.exe') @()
    exit 0
}
