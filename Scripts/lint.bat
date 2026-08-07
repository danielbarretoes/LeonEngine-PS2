@echo off
REM Scripts\lint.bat — format dry-run + Release /W4 Editor build
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."

call "%~dp0_vsenv.bat" vsdev quiet
if errorlevel 1 exit /b 1

set "CLANG_FORMAT="
for %%E in (Community Professional Enterprise) do (
  if not defined CLANG_FORMAT if exist "%ProgramFiles%\Microsoft Visual Studio\18\%%E\VC\Tools\Llvm\x64\bin\clang-format.exe" (
    set "CLANG_FORMAT=%ProgramFiles%\Microsoft Visual Studio\18\%%E\VC\Tools\Llvm\x64\bin\clang-format.exe"
  )
  if not defined CLANG_FORMAT if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Tools\Llvm\x64\bin\clang-format.exe" (
    set "CLANG_FORMAT=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Tools\Llvm\x64\bin\clang-format.exe"
  )
)
if not defined CLANG_FORMAT (
  echo ERROR: clang-format not found. See Docs/SETUP.md
  exit /b 1
)

echo === clang-format --dry-run ===
set "FMT_FAIL=0"
for %%D in (Engine Editor Runtime Plugins Tools Projects Tests Templates) do (
  if exist "%%D" for /r "%%D" %%f in (*.cpp *.h) do (
    set "P=%%f"
    echo !P!| findstr /I /C:"\build\" /C:"\build-" /C:"\_deps\" /C:"\_leon_" /C:"\.git\" >nul
    if errorlevel 1 (
      "%CLANG_FORMAT%" --dry-run --Werror "%%f" >nul 2>&1
      if errorlevel 1 (
        echo needs format: %%f
        set "FMT_FAIL=1"
      )
    )
  )
)
if "%FMT_FAIL%"=="1" (
  echo Format check failed. Run Scripts\format.bat
  exit /b 1
)
echo format OK

echo.
echo === MSVC /W4 Release build (Editor) ===
if not exist Editor\build (
  cmake -S Editor -B Editor/build -G "Visual Studio 18 2026" -A x64 2>nul
  if errorlevel 1 cmake -S Editor -B Editor/build -G "Visual Studio 17 2022" -A x64
  if errorlevel 1 exit /b 1
)
cmake --build Editor/build --config Release
if errorlevel 1 exit /b 1

echo lint OK
endlocal
