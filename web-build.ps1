param (
    [string]$config = "Release",         # "Debug" or "Release"
    [string]$emsdkPath = "C:/emsdk",
    [switch]$install = $false            # Install after building
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $emsdkPath)) {
    Write-Host "Could not find emsdk at $emsdkPath."
    Write-Host "Install emsdk or set emsdkPath appropriately."
    exit 1
}

if ($config -eq "Debug") {
    $buildPath = "out/build/Emscripten-Debug/"
    $installPath = "out/install/Emscripten-Debug/"
} elseif ($config -eq "Release") {
    $buildPath = "out/build/Emscripten-Release/"
    $installPath = "out/install/Emscripten-Release/"
} else {
    Write-Host "Invalid config. Use 'Debug' or 'Release'."
    exit 1
}
if (-not (Test-Path $buildPath)) {
    New-Item -ItemType Directory -Path $buildPath
}

$emsdkEnvPath = Join-Path $emsdkPath "emsdk_env.bat"
$emcmakePath = Join-Path $emsdkPath "upstream/emscripten/emcmake.bat"

Write-Host "Building..."
$env:EMSDK_QUIET = "1"
& $emsdkEnvPath
& $emcmakePath cmake . -B $buildPath `
    -DCMAKE_BUILD_TYPE="$config" -DCMAKE_INSTALL_PREFIX="$installPath" 

& ninja -C $buildPath

if ($install) {
    Write-Host "Installing..."
    if (-not (Test-Path $installPath)) {
        New-Item -ItemType Directory -Path $installPath
    }
    & cmake --install $buildPath
}