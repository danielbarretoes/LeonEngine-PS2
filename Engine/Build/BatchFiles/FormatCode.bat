@echo off
REM Engine\Build\BatchFiles\FormatCode.bat [--check]
REM clang-format every C++ file under Engine\Source, Engine\Platforms, Engine\Plugins and Game\*\Source.
REM Skips ThirdParty, Intermediate and Binaries. GLSL shaders are never touched.
REM --check: dry run, exit 1 if any file needs formatting (used by Lint.bat).
REM The repository is formatted with clang-format 20 (20.1.8 is the reference: pip install clang-format==20.1.8);
REM LEON_CLANG_FORMAT names the binary to use, else Visual Studio's, else the one on PATH. Another major version
REM formats a few constructs differently.
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0..\..\.."

set "MODE=-i"
if /I "%~1"=="--check" set "MODE=--dry-run --Werror"

set "CLANG_FORMAT=%LEON_CLANG_FORMAT%"
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
"%CLANG_FORMAT%" --version | findstr /C:"version 20." >nul
if errorlevel 1 echo WARNING: the repository is formatted with clang-format 20; this one may disagree.

REM File list (FOR /R cannot take a loop variable as its root, so use DIR /S /B).
set "LIST=%TEMP%\leon-format-files.txt"
if exist "%LIST%" del "%LIST%"
for %%D in (Engine\Source Engine\Platforms Engine\Plugins Game) do (
  if exist "%%D" dir /s /b /a-d "%%D\*.cpp" "%%D\*.h" "%%D\*.inl" 2>nul | findstr /V /I /C:"ThirdParty" /C:"Intermediate" /C:"Binaries" >> "%LIST%"
)
"%CLANG_FORMAT%" %MODE% --files="%LIST%"
if not "%ERRORLEVEL%"=="0" (
  echo FormatCode: files need formatting ^(run Engine\Build\BatchFiles\FormatCode.bat^)
  exit /b 1
)
echo FormatCode OK
endlocal
