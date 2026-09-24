@echo off
REM Scripts\format.bat — clang-format Engine/ Editor/ Runtime/ Plugins/ Tools/ Projects/ Tests/ Templates/
REM Skips build trees (build, build-*, _deps, _leon_*, .git).
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."

set "CLANG_FORMAT="
for %%E in (Community Professional Enterprise) do (
  if not defined CLANG_FORMAT if exist "%ProgramFiles%\Microsoft Visual Studio\18\%%E\VC\Tools\Llvm\x64\bin\clang-format.exe" (
    set "CLANG_FORMAT=%ProgramFiles%\Microsoft Visual Studio\18\%%E\VC\Tools\Llvm\x64\bin\clang-format.exe"
  )
  if not defined CLANG_FORMAT if exist "%ProgramFiles%\Microsoft Visual Studio\18\%%E\VC\Tools\Llvm\bin\clang-format.exe" (
    set "CLANG_FORMAT=%ProgramFiles%\Microsoft Visual Studio\18\%%E\VC\Tools\Llvm\bin\clang-format.exe"
  )
  if not defined CLANG_FORMAT if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Tools\Llvm\x64\bin\clang-format.exe" (
    set "CLANG_FORMAT=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Tools\Llvm\x64\bin\clang-format.exe"
  )
  if not defined CLANG_FORMAT if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Tools\Llvm\bin\clang-format.exe" (
    set "CLANG_FORMAT=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Tools\Llvm\bin\clang-format.exe"
  )
)

if not defined CLANG_FORMAT (
  echo ERROR: clang-format not found. See Docs/SETUP.md
  exit /b 1
)

echo Using: %CLANG_FORMAT%
echo Formatting Engine Runtime Plugins Tools Projects Tests Templates ...
for %%D in (Engine Runtime Plugins Tools Projects Tests) do (
  if exist "%%D" for /r "%%D" %%f in (*.cpp *.h) do (
    set "P=%%f"
    echo !P!| findstr /I /C:"\build\" /C:"\build-" /C:"\_deps\" /C:"\_leon_" /C:"\.git\" >nul
    if errorlevel 1 "%CLANG_FORMAT%" -i "%%f"
  )
)
echo Done.
endlocal
