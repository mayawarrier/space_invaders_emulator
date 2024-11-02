#!/bin/bash

configuration="Release" # "Debug" or "Release"
install=false # Run cmake --install after building
emsdkPath="$HOME/emsdk"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--configuration)
            configuration="$2"
            shift 2
            ;;
        -i|--install)
            install=true
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

if [[ $configuration == "Debug" ]]; then
    buildPath="build/Emscripten-Debug/"
    installPath="install/Emscripten-Debug/"
elif [[ $configuration == "Release" ]]; then
    buildPath="build/Emscripten-Release/"
    installPath="install/Emscripten-Release/"
else
    echo "Invalid configuration. Use 'Debug' or 'Release'."
    exit 1
fi

mkdir -p "$buildPath"

echo "Building..."
export EMSDK_QUIET="1"
source "$emsdkPath/emsdk_env.sh"
emcmake cmake . -B "$buildPath" \
    -DCMAKE_BUILD_TYPE="$configuration" -DCMAKE_INSTALL_PREFIX="$installPath"

make -C "$buildPath"

if $install; then
    echo "Installing..."
    cmake --install "$buildPath"
fi
