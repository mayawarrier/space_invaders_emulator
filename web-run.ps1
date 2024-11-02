param (
    [string]$configuration = "Release", # "Debug" or "Release"
    [string]$browser = "chrome",
    [switch]$fromInstall = $false, # Run from install folder instead of build
    [string]$emsdkPath = "E:/src/emsdk"
)

$runDir = if ($fromInstall) { "install" } else { "build" }

if ($configuration -eq "Debug") {
    $runPath = "out/$runDir/Emscripten-Debug/"
} elseif ($configuration -eq "Release") {
    $runPath = "out/$runDir/Emscripten-Release/"
} else {
    Write-Host "Invalid configuration. Use 'Debug' or 'Release'."
    exit 1
}

$indexPath = Join-Path $runPath "index.html"
if (-not (Test-Path $indexPath)) {
    if ($fromInstall) {
        & .\web-build.ps1 -configuration $configuration -emsdkBasePath $emsdkPath -install
    } else {
        & .\web-build.ps1 -configuration $configuration -emsdkBasePath $emsdkPath
    }
}

$emsdkEnvPath = Join-Path $emsdkPath "emsdk_env.bat"
$emrunPath = Join-Path $emsdkPath "upstream/emscripten/emrun.bat"

Write-Host "Running..."
$env:EMSDK_QUIET = "1"
& $emsdkEnvPath
& $emrunPath --browser $browser $runPath
