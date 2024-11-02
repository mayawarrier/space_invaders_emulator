param (
    [string]$configuration = "Release", # "Debug" or "Release"
    [switch]$install = $false, # Run cmake --install after building
    [string]$emsdkPath = "E:/src/emsdk"
)

if ($configuration -eq "Debug") {
    $buildPath = "out/build/Emscripten-Debug/"
    $installPath = "out/install/Emscripten-Debug/"
} elseif ($configuration -eq "Release") {
    $buildPath = "out/build/Emscripten-Release/"
    $installPath = "out/install/Emscripten-Release/"
} else {
    Write-Host "Invalid configuration. Use 'Debug' or 'Release'."
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
    -DCMAKE_BUILD_TYPE="$configuration" -DCMAKE_INSTALL_PREFIX="$installPath" 

& ninja -C $buildPath

if ($install) {
    Write-Host "Installing..."
    & cmake --install $buildPath
}