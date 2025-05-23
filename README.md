# space_invaders_emulator

Emulator for the classic Space Invaders arcade game from 1978!  
- Can run in a browser or as a native application
- Includes a complete emulation of the Intel 8080 microprocessor
- Supports original Space Invaders ROMs by Taito or Midway
- Cross-platform (Windows, Linux, MacOS, Web)

*This emulator includes the original Space Invaders ROM and audio and is hosted publicly for non-commercial, 
historical, and educational interest. All rights to the original game remain with the copyright holders.*

Play the game online at: https://mayawarrier.github.io/space_invaders_emulator/

## Screenshots
<p float="left">
<img src=".github/game2.png" width="250">
<img src=".github/game.png" width="250">
<img src=".github/settings.png" width="250">
</p>

## Build Prerequisites
- C++20 compiler (>= GCC 10, >= Clang 10, >= Visual Studio 2019)
- [CMake](https://cmake.org/) 3.15 or higher
- Python3 + [Emscripten SDK](https://github.com/emscripten-core/emsdk) for Web build
- [SDL2](https://github.com/libsdl-org/SDL) (>= 2.0.17) and 
  [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer) for native build

#### Installing SDL2 and SDL2_mixer
- These packages are only required for the native build.
- Installation is optional, they can be fetched and built automatically by the build script
  (pass -DALLOW_SDL2_SRCBUILD=ON to CMake)
- If you wish to install:
   - On Linux, add the following packages (or equivalent for your distro):   
	 libsdl2-2.0-0, libsdl2-dev, libsdl2-mixer-2.0-0, libsdl2-mixer-dev 
   - On Windows, download the [SDL2](https://github.com/libsdl-org/SDL/releases/download/release-2.30.3/SDL2-devel-2.30.3-VC.zip) 
	 and [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer/releases/download/release-2.8.0/SDL2_mixer-devel-2.8.0-VC.zip) dev libraries and extract
   all the files to folders named "SDL2" and "SDL2_mixer" respectively. 
   Place them in a [standard install location](https://cmake.org/cmake/help/latest/variable/CMAKE_SYSTEM_PREFIX_PATH.html#variable:CMAKE_SYSTEM_PREFIX_PATH) or in the same folder as the repo.

## Building
Once you have installed the prerequisites, run the commands below.

### Native Build
```
git clone https://github.com/mayawarrier/space_invaders_emulator.git
cd space_invaders_emulator
mkdir release
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX="../release"
cmake --build .
cmake --install .
```
This creates a Release build in the folder `release`. Use `-DCMAKE_BUILD_TYPE=Debug` for a Debug build.    
Pass `-DALLOW_SDL2_SRCBUILD=ON` to CMake to automatically fetch and build SDL2 libraries.

### Web Build
Follow [these instructions](https://emscripten.org/docs/getting_started/downloads.html) to install Emscripten using emsdk.     
Then run the commands below, replacing `<path-to-emsdk>` with the correct path:
```
git clone https://github.com/mayawarrier/space_invaders_emulator.git
cd space_invaders_emulator
web/scripts/./web-build -Install -EmsdkPath <path-to-emsdk>
```
This creates a Release build in the folder `out/install/Emscripten-Release`. Use `-BuildType Debug` for a Debug build.      
You can run the emulator with `web/scripts/./web-run -Install -Browser chrome` (starts a development server).

## Run options
```
1978 Space Invaders emulator.
Usage:
  spaceinvaders [OPTION...]

  -h, --help             Show this help message.
  -a, --asset-dir <dir>  Path to game assets (ROM/audio/fonts etc.)
                         (default: assets/)
  -r, --renderer <rend>  Render backend to use. See SDL_HINT_RENDER_DRIVER.
                         If not provided, will be determined automatically.
      --disable-menu     Disable menu bar.

```
