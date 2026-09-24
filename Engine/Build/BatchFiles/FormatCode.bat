@echo off
REM Engine\Build\BatchFiles\FormatCode.bat [--check]
REM clang-format every C++ file under Engine\Source, Engine\Platforms, Engine\Plugins and Game\*\Source.
REM Skips ThirdParty, Intermediate and Binaries. GLSL shaders are never touched.
REM --check: dry run, exit 1 if any file needs formatting (used by Lint.bat).
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\..\.."

set "MODE=-i"
if /I "%~1"=="--check" set "MODE=--dry-run --Werror"

set "CLANG_FORMAT="
for %%V in (18 2022) do for %%E in (Community Professional Enterprise) do (
  if not defined CLANG_FORMAT if exist "%ProgramFiles%\Microsoft Visual Studio\%%V\%%E\VC\Tools\Llvm\x64\bin\clang-format.exe" (
    set "CLANG_FORMAT=%ProgramFiles%\Microsoft Visual Studio\%%V\%%E\VC\Tools\Llvm\x64\bin\clang-format.exe"
  )
)
if not defined CLANG_FORMAT (
  where clang-format >nul 2>&1 && set "CLANG_FORMAT=clang-format"
)
if not defined CLANG_FORMAT (
  echo ERROR: clang-format not found. See Docs/SETUP.md
  exit /b 1
)

set "FAILED=0"
for %%D in (Engine\Source Engine\Platforms Engine\Plugins Game) do (
  if exist "%%D" for /r "%%D" %%f in (*.cpp *.h *.inl) do (
    set "P=%%f"
    echo !P!| findstr /I /C:"\ThirdParty\" /C:"\Intermediate\" /C:"\Binaries\" >nul
    if errorlevel 1 (
      "%CLANG_FORMAT%" %MODE% "%%f" >nul 2>&1
      if errorlevel 1 (
        echo needs format: %%f
        set "FAILED=1"
      )
    )
  )
)
if "%FAILED%"=="1" exit /b 1
echo FormatCode OK
endlocal
