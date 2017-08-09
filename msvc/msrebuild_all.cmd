@echo off
rem call "c:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"

"c:\Program Files (x86)\MSBuild\14.0\bin\msbuild.exe" Oricutron.vcxproj /t:Rebuild /p:Configuration=Debug;Platform=Win32
"c:\Program Files (x86)\MSBuild\14.0\bin\msbuild.exe" Oricutron.vcxproj /t:Rebuild /p:Configuration=Release;Platform=Win32

"c:\Program Files (x86)\MSBuild\14.0\bin\msbuild.exe" Oricutron.vcxproj /t:Rebuild /p:Configuration=Debug;Platform=x64
"c:\Program Files (x86)\MSBuild\14.0\bin\msbuild.exe" Oricutron.vcxproj /t:Rebuild /p:Configuration=Release;Platform=x64
