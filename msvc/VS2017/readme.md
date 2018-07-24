# Oricutron 64-Bit (x64) - Microsoft Visual C++ 2017

Compiling Oricutron with Microsoft Visual Studio Community 2017 Version 15.6.0 Preview 7

## Libraries

### SDL 2

Depends on SDL2 from vcpkg, see https://docs.microsoft.com/en-us/cpp/vcpkg

## Build Oricutron

   1. Open the solution "Oricutron.sln"
   2. Open "Build -> Batch Build..." press "Select All" and press "Rebuild"

   or (alternative) to steps 1. and 2.:
      Execute
      "c:\Program Files (x86)\Microsoft Visual Studio\Preview\Community\VC\Auxiliary\Build\vcvars64.bat" and
      "msrebuild_all.cmd" from the "msvc" folder.

   3. Call make_links.cmd from the "msvc" folder as Administrator to set the necassary links to vcpkg, roms, images and oricutron.cfg
      to be able to debug / run Oricutron.

## Known Issues

   1. APP_NAME_FULL not set to git revision
   2. Lots of warnings
