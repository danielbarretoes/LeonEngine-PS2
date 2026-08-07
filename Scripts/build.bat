@echo off
REM Build Leon Editor → Editor\build\ (VS generator + tests)
setlocal EnableExtensions
cd /d "%~dp0.."

call "%~dp0_vsenv.bat" vsdev need-git
if errorlevel 1 exit /b 1

cmake -S Editor -B Editor/build -G "Visual Studio 18 2026" -A x64 2>nul
if errorlevel 1 (
  echo Trying Visual Studio 17 2022 generator...
  cmake -S Editor -B Editor/build -G "Visual Studio 17 2022" -A x64
  if errorlevel 1 exit /b 1
)

cmake --build Editor/build --config Release --parallel
if errorlevel 1 exit /b 1

echo.
echo Editor built under Editor\build\Release\
echo   LeonEngine.exe  leon_tests.exe
echo PS2 Lab:  Scripts\build-ps2-docker.ps1 lab
echo Tools:    cmake -S Tools -B Tools/build
endlocal
