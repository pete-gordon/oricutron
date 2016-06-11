# Oricutron x86 / x64 - Microsoft Visual C++ 2015

Compiling Oricutron with Microsoft Visual Studio Community 2015 Update 2

## Libraries

### SDL 2

   1. Download SDL source libraries for Visual C++ 32/64-bit from here: https://www.libsdl.org/index.php
   2. Uncompress SDL into your favorite directory e.g. T:\ThirdParty\Libs\SDL\...
   3. Open "VisualC\SDL.sln" and confirm conversion.
   4. Open "Build\Batch Build..." select all and press "Rebuild"
   5. Edit and execute "make_links.cmd" from the "msvc" folder as administrator.

## Build Oricutron

   1. Open "Oricutron.sln"
   2. Open "Build\Batch Build..." select all and press "Rebuild".
   2. (alternative) Execute "msrebuild_all.cmd" script from the "msvc" folder.

## Known Issues

   1. 6551.h BACKEND_MODEM disabled, because of linker errors
   2. APP_NAME_FULL not set to git revision
   3. Lots of warnings
