@echo off
rem Must be executed as administrator because calling mklink

set IMAGES="..\images"
set ROMS="..\roms"
set CFG="..\oricutron.cfg"

rem SDL
if not exist "sdl2" ( mklink /d "sdl2" "..\..\..\Lib\SDL\SDL2-2.0.4_src" )
if not exist "sdl" ( mklink /d "sdl" "sdl2" )

if not exist "roms" ( mklink /D "roms" %ROMS% )
if not exist "images" ( mklink /D "images" %IMAGES% )
if not exist "oricutron.cfg" ( mklink "oricutron.cfg" %CFG% )

rem x86
if not exist "Debug" ( md "Debug" )
if not exist "Debug\SDL2.dll" ( copy "sdl\VisualC\Win32\Debug\SDL2.dll" "Debug\SDL2.dll" )
if not exist "Debug\roms" ( mklink /D "Debug\roms" %ROMS% )
if not exist "Debug\images" ( mklink /D "Debug\images" %IMAGES% )
if not exist "Debug\oricutron.cfg" ( mklink "Debug\oricutron.cfg" %CFG% )

if not exist "Release" ( md "Release" )
if not exist "Release\SDL2.dll" ( copy "sdl\VisualC\Win32\Release\SDL2.dll" "Release\SDL2.dll" )
if not exist "Release\roms" ( mklink /D "Release\roms" %ROMS% )
if not exist "Release\images" ( mklink /D "Release\images" %IMAGES% )
if not exist "Release\oricutron.cfg" ( mklink "Release\oricutron.cfg" %CFG% )

rem x64
if not exist "x64\roms" ( mklink /D "x64\roms" %ROMS% )
if not exist "x64\images" ( mklink /D "x64\images" %IMAGES% )
if not exist "x64\Debug" ( md "x64\Debug" )
if not exist "x64\Debug\SDL2.dll" ( copy "sdl\VisualC\x64\Debug\SDL2.dll" "x64\Debug\SDL2.dll" )
if not exist "x64\Debug\roms" ( mklink /D "x64\Debug\roms" %ROMS% )
if not exist "x64\Debug\images" ( mklink /D "x64\Debug\images" %IMAGES% )
if not exist "x64\Debug\oricutron.cfg" ( mklink "x64\Debug\oricutron.cfg" %CFG% )

if not exist "x64\Release" ( md "x64\Release" )
if not exist "x64\Release\SDL2.dll" ( copy "sdl\VisualC\x64\Release\SDL2.dll" "x64\Release\SDL2.dll" )
if not exist "x64\Release\roms" ( mklink /D "x64\Release\roms" %ROMS% )
if not exist "x64\Release\images" ( mklink /D "x64\Release\images" %IMAGES% )
if not exist "x64\Release\oricutron.cfg" ( mklink "x64\Release\oricutron.cfg" %CFG% )
