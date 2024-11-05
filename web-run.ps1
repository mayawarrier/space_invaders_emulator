param (
    [string]$config = "Release",         # "Debug" or "Release"
    [string]$browser = "chrome",
    [string]$emsdkPath = "C:/emsdk",
    [switch]$build = $false,             # Build before running
    [switch]$install = $false            # Install before running
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $emsdkPath)) {
    Write-Host "Could not find emsdk at $emsdkPath."
    Write-Host "Install emsdk or set emsdkPath appropriately."
    exit 1
}

$runDir = if ($install) { "install" } else { "build" }

if ($config -eq "Debug") {
    $runPath = "out/$runDir/Emscripten-Debug/"
} elseif ($config -eq "Release") {
    $runPath = "out/$runDir/Emscripten-Release/"
} else {
    Write-Host "Invalid config. Use 'Debug' or 'Release'."
    exit 1
}

if ($install) {
    & .\web-build.ps1 -config $config -emsdkPath $emsdkPath -install
} elseif ($build) {
    & .\web-build.ps1 -config $config -emsdkPath $emsdkPath
}

$emsdkEnvPath = Join-Path $emsdkPath "emsdk_env.bat"
$emrunPath = Join-Path $emsdkPath "upstream/emscripten/emrun.bat"

Write-Host "Running..."
$env:EMSDK_QUIET = "1"
& $emsdkEnvPath
& $emrunPath --browser $browser $runPath
