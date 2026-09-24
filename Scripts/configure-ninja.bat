@echo off
REM Scripts\configure-ninja.bat — Ninja + compile_commands → Editor\build-ninja\ + Tools\build-ninja\
REM Tree role: clangd DB + full Editor Ninja build (tests default ON from Editor CMake).
REM Day-to-day edit without tests: Scripts\build-fast.bat → Editor\build-fast\
setlocal EnableExtensions
cd /d "%~dp0.."

call "%~dp0_vsenv.bat" vcvars need-ninja need-git
if errorlevel 1 exit /b 1

cmake -S Engine -B Engine/build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release -DLEON_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON || exit /b 1
cmake -S Tools -B Tools/build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON || exit /b 1
cmake --build Engine/build-ninja -j %NUMBER_OF_PROCESSORS% || exit /b 1

REM clangd breaks on MSVC `-IC:\Users\...` escapes; normalize to forward slashes.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0fix-compile-commands.ps1"
if errorlevel 1 echo WARNING: could not normalize compile_commands.json for clangd

echo Done. Binaries + compile_commands.json in Editor\build-ninja\
echo Editor: Editor\build-ninja\LeonEngine.exe
echo clangd: Engine/build-ninja + Tools/build-ninja (see .clangd / Docs/SETUP.md)
endlocal
