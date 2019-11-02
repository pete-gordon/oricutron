# Nina + CMake Buildsystem for Oricutron

## Dependencies

### Windows

SDL2, OpenGL

### Linux

SDL2, GTK-3

## Building

### Windows

Open a Visual Studio Developer Command Prompt 2019

`mkdir build\windows`
`cd build\windows`
`cmake -GNinja ../..`
`ninja`
`Oricutron.exe`

### Linux

Open a terminal (on Windows you can use WSL + e.g. Ubuntu)

`mkdir build/linux`
`cd build/linux`
`cmake -GNinja ../..`
`ninja`
`./oricutron`

### Linux + Windows

Before starting Oricutron you must copy the roms and images directories and the configuration file

to the `build/windows` and/or `build/linux` directory.

On Windows with WSL you need VcXsrv X server for Windows to see the output.

