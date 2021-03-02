# Ninja + CMake Buildsystem for Oricutron

## Dependencies

### Windows

SDL2, OpenGL

### Linux

SDL2, GTK-3

## Building

### Windows

Open a Visual Studio Developer Command Prompt 2019

```shell
mkdir build\windows
cd build\windows
cmake -GNinja ..\.. [-DUSE_SDL2:BOOL=ON]
ninja
copy Oricutron.exe ..\..
[copy Oricutron-sdl2.exe ..\..]
cd ..\..
mklink SDL.dll msvc\vcpkg\installed\x64-windows\bin\SDL.dll
mklink SDL2.dll msvc\vcpkg\installed\x64-windows\bin\SDL2.dll
Oricutron.exe
[Oricutron-sdl2.exe]
```

### Linux

Open a terminal (on Windows you can use WSL + e.g. Ubuntu)

```shell
mkdir -p build/linux
cd build/linux
cmake -GNinja ../..
[cmake -GNinja ../.. -DUSE_SDL2:BOOL=ON]
ninja
cp Oricutron ../..
[cp Oricutron-sdl2 ../..]
cd ../..
./Oricutron
[./Oricutron-sdl2]
```

### Linux + Windows

Before starting Oricutron you must copy the roms and images directories and the configuration file

to the `build/windows` and/or `build/linux` directory.

On Windows with WSL you need VcXsrv X server for Windows to see the output.

