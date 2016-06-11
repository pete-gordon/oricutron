# Oricutron x86 / x64 - Microsoft Visual C++ 2015

Compiling Oricutron with Microsoft Visual Studio Community 2015 Update 2

## Libraries

### SDL 2

Download SDL source libraries for Visual C++ 32/64-bit from here: https://www.libsdl.org/index.php
Uncompress SDL into your favorite directory e.g. T:\ThirdParty\Libs\SDL\...
Open VisualC\SDL.sln and confirm conversion.
Open "Build\Batch Build..." select all and press "Rebuild"
Edit and execute make_links.cmd as administrator.

## Known Issues

   1. 6551.h BACKEND_MODEM disabled, because of linker errors
   2. APP_NAME_FULL not set to git revision
   3. Lot of warnings
