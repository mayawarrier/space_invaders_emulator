#!/bin/sh

set -e

config="Release"        # "Debug" or "Release"
browser="chrome"
emsdkPath="$HOME/emsdk"
build=false             # Build before running
install=false           # Install before running

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
        -r|--browser)
            if [ -z "$2" ]; then
                echo "Error: --browser requires a value."
                exit 1
            fi
            browser="$2"
            shift 2
            ;;
        -i|--install)
            install=true
            shift
            ;;
        -b|--build)
            build=true
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

if [ "$install" = "true" ]; then 
    runDir="install"
else 
    runDir="build"
fi

if [ "$config" = "Debug" ]; then
    runPath="out/$runDir/Emscripten-Debug/"
elif [ "$config" = "Release" ]; then
    runPath="out/$runDir/Emscripten-Release/"
else
    echo "Invalid config. Use 'Debug' or 'Release'."
    exit 1
fi

SCRIPT_DIR=$(dirname "$0")

if [ "$install" = "true" ]; then
    "$SCRIPT_DIR/web-build.sh" --config "$config" --emsdk-path "$emsdkPath" --install
elif [ "$build" = "true" ]; then
    "$SCRIPT_DIR/web-build.sh" --config "$config" --emsdk-path "$emsdkPath"
fi

echo "Running..."
export EMSDK_QUIET="1"
. "$emsdkPath/emsdk_env.sh"
emrun --browser "$browser" "$runPath"
