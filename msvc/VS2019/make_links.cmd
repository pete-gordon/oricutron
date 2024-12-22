@echo off
rem Must be executed as Administrator because of calling mklink.
rem Depends on vcpkg, see https://docs.microsoft.com/en-us/cpp/vcpkg
rem Oricutron needs some roms installed in the roms directory, else the start fails.

rem --- Edit -------------------------------------------------------------------------------

rem --- Do not edit -------------------------------------------------------------------------------

rem SDL
set VCPKG=..\vcpkg\installed\x64-windows

set IMAGES="..\..\images"
set ROMS="..\..\roms"
set CFG="..\..\oricutron.cfg"

if not exist "roms" ( mklink /D "roms" %ROMS% )
if not exist "images" ( mklink /D "images" %IMAGES% )
if not exist "oricutron.cfg" ( mklink "oricutron.cfg" %CFG% )

rem x64
if not exist "x64" md "x64"
if not exist "x64\roms" ( mklink /D "x64\roms" "..\..\..\roms" )
if not exist "x64\images" ( mklink /D "x64\images" "..\..\..\images" )

rem x64\Debug
if not exist "x64\Debug" md "x64\Debug"
if not exist "x64\Debug" ( md "x64\Debug" )
if not exist "x64\Debug\SDL2.dll" ( copy "%VCPKG%\debug\bin\SDL2d.dll" "x64\Debug\SDL2.dll" )
if not exist "x64\Debug\roms" ( mklink /D "x64\Debug\roms" %ROMS% )
if not exist "x64\Debug\images" ( mklink /D "x64\Debug\images" %IMAGES% )
if not exist "x64\Debug\oricutron.cfg" ( mklink "x64\Debug\oricutron.cfg" %CFG% )

rem x64\Release
if not exist "x64\Release" md "x64\Release"
if not exist "x64\Release" ( md "x64\Release" )
if not exist "x64\Release\SDL2.dll" ( copy "%VCPKG%\bin\SDL2.dll" "x64\Release\SDL2.dll" )
if not exist "x64\Release\roms" ( mklink /D "x64\Release\roms" %ROMS% )
if not exist "x64\Release\images" ( mklink /D "x64\Release\images" %IMAGES% )
if not exist "x64\Release\oricutron.cfg" ( mklink "x64\Release\oricutron.cfg" %CFG% )


rem x64\Debug SDL1
if not exist "x64\Debug SDL1" md "x64\Debug"
if not exist "x64\Debug SDL1" ( md "x64\Debug" )
if not exist "x64\Debug SDL1\SDL.dll" ( copy "%VCPKG%\debug\bin\SDL.dll" "x64\Debug SDL1\SDL.dll" )
if not exist "x64\Debug SDL1\roms" ( mklink /D "x64\Debug\roms" %ROMS% )
if not exist "x64\Debug SDL1\images" ( mklink /D "x64\Debug\images" %IMAGES% )
if not exist "x64\Debug SDL1\oricutron.cfg" ( mklink "x64\Debug\oricutron.cfg" %CFG% )

rem x64\Release SDL1
if not exist "x64\Release SDL1" md "x64\Release"
if not exist "x64\Release SDL1" ( md "x64\Release" )
if not exist "x64\Release SDL1\SDL.dll" ( copy "%VCPKG%\bin\SDL.dll" "x64\Release SDL1\SDL.dll" )
if not exist "x64\Release SDL1\roms" ( mklink /D "x64\Release\roms" %ROMS% )
if not exist "x64\Release SDL1\images" ( mklink /D "x64\Release\images" %IMAGES% )
if not exist "x64\Release SDL1\oricutron.cfg" ( mklink "x64\Release\oricutron.cfg" %CFG% )
