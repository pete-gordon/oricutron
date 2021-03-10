# Ninja + CMake Buildsystem for Oricutron

## Symbols used in this documentation

[...] Optional steps

## Dependencies

### Windows

SDL1/2, OpenGL

### Linux

SDL1/2, GTK-3

## Preparation the Build System

### UBUNTU 18.04 / 20.04

```shell
sudo apt-get update
sudo apt install build-essential
sudo apt install cmake
sudo apt install ninja-build
sudo apt install libsdl1.2-dev
[sudo apt install libsdl2-dev]
sudo apt install libgtk-3-dev
```

## Building

### Windows + Visual Studio 2019 + cmake + Ninja

Open a Visual Studio Developer Command Prompt 2019

```shell
mkdir build\windows
pushd build\windows
cmake -GNinja ..\.. [-DUSE_SDL2:BOOL=ON]
ninja
copy Oricutron.exe ..\..
[copy Oricutron-sdl2.exe ..\..]
popd
mklink SDL.dll msvc\vcpkg\installed\x64-windows\bin\SDL.dll
[mklink SDL2.dll msvc\vcpkg\installed\x64-windows\bin\SDL2.dll]
Oricutron.exe
[Oricutron-sdl2.exe]
```

### Linux + cmake + Ninja

Open a terminal (on Windows you can use e.g. WSL + Ubuntu)

```shell
mkdir -p build/linux
pushd build/linux
cmake -GNinja ../..
[cmake -GNinja ../.. -DUSE_SDL2:BOOL=ON]
ninja
cp Oricutron ../..
[cp Oricutron-sdl2 ../..]
popd
./Oricutron
[./Oricutron-sdl2]
```

## Remarks Linux + Windows

Before starting Oricutron you must copy your roms, disks and images to the appropriate folders and change the configuration file `oricutron.cfg` to your needs.

On Windows with WSL you need VcXsrv X server for Windows to see the output.
