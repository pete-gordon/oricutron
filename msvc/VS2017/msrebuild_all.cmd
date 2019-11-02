@echo off
rem call "c:\Program Files (x86)\Microsoft Visual Studio\Preview\Community\VC\Auxiliary\Build\vcvars64.bat"

set MSBUILDTOOL=msbuild.exe
"%MSBUILDTOOL%" Oricutron.vcxproj /t:Rebuild /p:Configuration=Debug;Platform=x64
"%MSBUILDTOOL%" Oricutron.vcxproj /t:Rebuild /p:Configuration=Release;Platform=x64
