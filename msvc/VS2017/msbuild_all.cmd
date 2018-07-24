@echo off
rem call "c:\Program Files (x86)\Microsoft Visual Studio\Preview\Community\VC\Auxiliary\Build\vcvars64.bat"

set MSBUILDTOOL=c:\Program Files (x86)\Microsoft Visual Studio\Preview\Community\MSBuild\15.0\Bin\msbuild.exe
"%MSBUILDTOOL%" Oricutron.vcxproj /t:Build /p:Configuration=Debug;Platform=x64
"%MSBUILDTOOL%" Oricutron.vcxproj /t:Build /p:Configuration=Release;Platform=x64
