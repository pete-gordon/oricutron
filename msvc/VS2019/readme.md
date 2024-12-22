# Oricutron 64-Bit (x64) - Microsoft Visual Studio 2019 Preview

Compiling Oricutron with Microsoft Visual Studio 2019 Preview

## Libraries

### SDL 2

Depends on SDL2 from vcpkg, see https://docs.microsoft.com/en-us/cpp/vcpkg

## Build Oricutron
   1. Call make_links.cmd from the "msvc/VS2019" folder as Administrator
      to set the necassary links to vcpkg, roms, images and oricutron.cfg
      and to be able to debug / run Oricutron.

   2. Open the solution "Oricutron.sln"
   3. Open "Build -> Batch Build..." and press "Select All" and press "Rebuild"

   or (alternative) to steps 2. and 3.:
      Open a Developer Command Prompt 2019 and
      execute "msrebuild_all.cmd" from the "msvc/VS2019" folder.

## Known Issues

   1. APP_NAME_FULL not set to git revision
   2. Lots of warnings
