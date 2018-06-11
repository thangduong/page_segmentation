@echo off
WHERE msbuild >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
echo Please run this in Visual Studio build console.  
echo Find "Developer command prompt" in the start menu.
) else (
git submodule update --init --recursive 
cd gflags
cmake .
msbuild gflags.sln /property:Configuration=Release
cd ..\segment
cmake -G "Visual Studio 15 2017 Win64" .
msbuild segment.sln /property:Configuration=Release
cd ..
)