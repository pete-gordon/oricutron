@echo off
rem call "c:\Program Files (x86)\Microsoft Visual Studio\2019\Preview\VC\Auxiliary\Build\vcvars64.bat"

set MSBUILDTOOL=msbuild.exe
"%MSBUILDTOOL%" Oricutron.vcxproj /t:Build /p:Configuration=Debug;Platform=x64
"%MSBUILDTOOL%" Oricutron.vcxproj /t:Build /p:Configuration=Release;Platform=x64
