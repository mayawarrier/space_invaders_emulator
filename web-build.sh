#!/bin/sh

set -e

config="Release"        # "Debug" or "Release"
install=false           # Run cmake --install after building
emsdkPath="$HOME/emsdk"

# Parse command line arguments
while [ "$#" -gt 0 ]; do
    case "$1" in
        -c|--config)
            if [ -z "$2" ]; then
                echo "Error: --config requires a value."
                exit 1
            fi
            config="$2"
            shift 2
            ;;
        -i|--install)
            install=true
            shift
            ;;
        -e|--emsdk-path)
            if [ -z "$2" ]; then
                echo "Error: --emsdk-path requires a value."
                exit 1
            fi
            emsdkPath="$2"
            shift 2
            ;;
        *)
            echo "Error: Unrecognized option $1"
            exit 1
            ;;
    esac
done

if [ ! -d "$emsdkPath" ]; then
    echo "Could not find emsdk at $emsdkPath."
    echo "Install emsdk or set emsdk-path appropriately."
    exit 1
fi

if [ "$config" = "Debug" ]; then
    buildPath="out/build/Emscripten-Debug/"
    installPath="out/install/Emscripten-Debug/"
elif [ "$config" = "Release" ]; then
    buildPath="out/build/Emscripten-Release/"
    installPath="out/install/Emscripten-Release/"
else
    echo "Invalid config. Use 'Debug' or 'Release'."
    exit 1
fi

mkdir -p "$buildPath"

echo "Building..."
export EMSDK_QUIET="1"
. "$emsdkPath/emsdk_env.sh"
emcmake cmake . -B "$buildPath" \
    -DCMAKE_BUILD_TYPE="$config" -DCMAKE_INSTALL_PREFIX="$installPath"

make -C "$buildPath"

if [ "$install" = "true" ]; then
    echo "Installing..."
    mkdir -p "$installPath"
    cmake --install "$buildPath"
fi
