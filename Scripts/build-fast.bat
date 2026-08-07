@echo off
REM Fast incremental Editor build: Ninja + Release + tests OFF.
REM Output: Editor\build-fast\LeonEngine.exe
REM Tree role: day-to-day edit (not tests / not clangd DB — see build-ninja).
setlocal EnableExtensions
cd /d "%~dp0.."

call "%~dp0_vsenv.bat" vcvars need-ninja need-git
if errorlevel 1 exit /b 1

if not exist Editor\build-fast\build.ninja goto Configure
goto Build

:Configure
echo Configuring Editor\build-fast (Ninja, Release, LEON_BUILD_TESTS=OFF)...
cmake -S Editor -B Editor/build-fast -G Ninja -DCMAKE_BUILD_TYPE=Release -DLEON_BUILD_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
if errorlevel 1 exit /b 1

:Build
echo Building leon-editor ^(LeonEngine.exe^)...
cmake --build Editor/build-fast -j %NUMBER_OF_PROCESSORS% --target leon-editor
if errorlevel 1 exit /b 1

echo.
echo Done: Editor\build-fast\LeonEngine.exe
endlocal
