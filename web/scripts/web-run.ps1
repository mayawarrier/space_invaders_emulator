param (
    [string]$RunType = "Release",        # "Debug" or "Release"
    [string]$Browser = "chrome",
    [switch]$Build = $false,             # Build before running
    [switch]$Install = $false,           # Install before running
    [string]$EmsdkPath = "C:/emsdk"
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

$runDir = if ($Install) { "install" } else { "build" }

if ($RunType -eq "Debug") {
    $runPath = "out/$runDir/Emscripten-Debug/"
} elseif ($RunType -eq "Release") {
    $runPath = "out/$runDir/Emscripten-Release/"
} else {
    Write-Host "Invalid RunType. Use 'Debug' or 'Release'."
    exit 1
}

if ($Install) {
    & "$PSScriptRoot\web-build.ps1" -BuildType $RunType -emsdkPath $EmsdkPath -install
    Assert-Success
} elseif ($Build) {
    & "$PSScriptRoot\web-build.ps1" -BuildType $RunType -emsdkPath $EmsdkPath
    Assert-Success
}

Write-Host "------------------- Running..."

$env:EMSDK_QUIET = "1"

$emsdkEnvPath = Join-Path $EmsdkPath "emsdk_env.bat"
& $emsdkEnvPath
Assert-Success "emsdk_env.bat failed."

$emrunPath = Join-Path $EmsdkPath "upstream/emscripten/emrun.bat"
& $emrunPath --browser $Browser $runPath
Assert-Success "emrun.bat failed."
