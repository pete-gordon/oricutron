# Oricutron x86 / x64 - Microsoft Visual C++ 2015

Compiling Oricutron with Microsoft Visual Studio Community 2015 Update 2

## Libraries

### SDL 2

You need to recompile the SDL yourself, because the binaries are build with an older Visual Studio.

   1. Download SDL 2.x source libraries for Visual C++ 32/64-bit from here: https://www.libsdl.org/index.php
   2. Uncompress SDL into your favorite developement directory.
   3. Open the solution "SDL.sln" from the directory "VisualC" and confirm conversion.
   4. Open "Build -> Batch Build..." select all and press "Rebuild"
   5. Edit and execute "make_links.cmd" from the "msvc" folder as administrator.

## Build Oricutron

   1. Open the solution "Oricutron.sln"
   2. Open "Build -> Batch Build..." select all and press "Rebuild".
   2. (alternative) Execute "msrebuild_all.cmd" script from the "msvc" folder.

## Known Issues

   1. 6551.h BACKEND_MODEM disabled, because of linker errors
   2. APP_NAME_FULL not set to git revision
   3. Lots of warnings
