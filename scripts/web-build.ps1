param (
    [string]$BuildType = "Release",      # "Debug" or "Release"
    [string]$EmsdkPath = "C:/emsdk",
    [switch]$Install = $false            # Install after building
)

$ErrorActionPreference = "Stop"

function Assert-Success {
    param ([string]$ErrorMessage = $null)

    if ($LASTEXITCODE -ne 0) {
        if ($ErrorMessage) { Write-Host $ErrorMessage }
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path $EmsdkPath)) {
    Write-Host "Could not find emsdk at $EmsdkPath."
    Write-Host "Install emsdk or set EmsdkPath appropriately."
    exit 1
}

if ($BuildType -eq "Debug") {
    $buildPath = "out/build/Emscripten-Debug/"
    $installPath = "out/install/Emscripten-Debug/"
} elseif ($BuildType -eq "Release") {
    $buildPath = "out/build/Emscripten-Release/"
    $installPath = "out/install/Emscripten-Release/"
} else {
    Write-Host "Invalid BuildType. Use 'Debug' or 'Release'."
    exit 1
}
if (-not (Test-Path $buildPath)) {
    New-Item -ItemType Directory -Path $buildPath | Out-Null
}

Write-Host "------------------- Building..."

$env:EMSDK_QUIET = "1"

$emsdkEnvPath = Join-Path $EmsdkPath "emsdk_env.bat"
& $emsdkEnvPath
Assert-Success "emsdk_env.bat failed."

$emcmakePath = Join-Path $EmsdkPath "upstream/emscripten/emcmake.bat"
& $emcmakePath cmake . -B $buildPath `
    -DCMAKE_BUILD_TYPE="$BuildType" -DCMAKE_INSTALL_PREFIX="$installPath" 
Assert-Success "cmake configure failed."

& ninja -C $buildPath
Assert-Success "ninja build failed."

if ($install) {
    Write-Host "------------------- Installing..."
    if (-not (Test-Path $installPath)) {
        New-Item -ItemType Directory -Path $installPath | Out-Null
    }
    & cmake --install $buildPath
    Assert-Success "cmake install failed."
}