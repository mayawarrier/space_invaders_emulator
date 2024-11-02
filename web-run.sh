#!/bin/bash

configuration="Release" # "Debug" or "Release"
browser="chrome"
fromInstall=false # Run from install folder instead of build
emsdkPath="$HOME/emsdk"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--configuration)
            configuration="$2"
            shift 2
            ;;
        -b|--browser)
            browser="$2"
            shift 2
            ;;
        -f|--from-install)
            fromInstall=true
            shift
            ;;
        -e|--emsdk-path)
            emsdkPath="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

runDir=$([[ $fromInstall == true ]] && echo "install" || echo "build")

if [[ $configuration == "Debug" ]]; then
    runPath="out/$runDir/Emscripten-Debug/"
elif [[ $configuration == "Release" ]]; then
    runPath="out/$runDir/Emscripten-Release/"
else
    echo "Invalid configuration. Use 'Debug' or 'Release'."
    exit 1
fi

indexPath="$runPath/index.html"
if [[ ! -f $indexPath ]]; then
    echo "Building..."
    ./web-build.sh --configuration "$configuration" --emsdk-path "$emsdkPath" ${fromInstall:+--install}
fi

echo "Running..."
export EMSDK_QUIET="1"
source "$emsdkPath/emsdk_env.sh"
emrun --browser "$browser" "$runPath"
