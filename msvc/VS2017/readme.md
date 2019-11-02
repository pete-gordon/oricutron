# Oricutron 64-Bit (x64) - Microsoft Visual C++ 2017

Compiling Oricutron with Microsoft Visual Studio Community 2017 Version 15.6.0 Preview 7

## Libraries

### SDL 2

Depends on SDL2 from vcpkg, see https://docs.microsoft.com/en-us/cpp/vcpkg.
vspkg and SDL2 will be fetched automatically as long as git is accessible in the build environment.
Otherwise, run "git submodule update --init" prior to building the solution.

## Build Oricutron

   1. Open the solution "Oricutron.sln"
   2. Build through Visual Studio. Allow time on the first build to fetch dependencies.
   
   or (alternative) to steps 1. and 2.:
      Execute
      "c:\Program Files (x86)\Microsoft Visual Studio\Preview\Community\VC\Auxiliary\Build\vcvars64.bat" and
      "msrebuild_all.cmd" from the "msvc" folder.

## Known Issues

   1. APP_NAME_FULL not set to git revision
   2. Lots of warnings
