@echo off
REM Scripts\_vsenv.bat — shared MSVC env + CMake PATH for Leon Scripts.
REM Call from another script (after setlocal / cd to repo root):
REM   call "%~dp0_vsenv.bat" vcvars
REM   call "%~dp0_vsenv.bat" vsdev
REM   call "%~dp0_vsenv.bat" vsdev quiet
REM Optional trailing tokens (any order after mode):
REM   quiet       — suppress VsDevCmd / vcvars stdout
REM   optional    — if VS missing, exit 0 without setting env (package-editor)
REM   need-ninja  — require ninja on PATH
REM   need-git    — require git on PATH
REM
REM Modes:
REM   vcvars — Ninja / clangd trees (build-fast, configure-ninja)
REM   vsdev  — VS generator / general (build, test, cook, lint, build-project)

set "_LEON_MODE="
set "_LEON_QUIET=0"
set "_LEON_OPTIONAL=0"
set "_LEON_NEED_NINJA=0"
set "_LEON_NEED_GIT=0"

:ParseArgs
if "%~1"=="" goto Parsed
if /I "%~1"=="vcvars" (
  set "_LEON_MODE=vcvars"
  shift
  goto ParseArgs
)
if /I "%~1"=="vsdev" (
  set "_LEON_MODE=vsdev"
  shift
  goto ParseArgs
)
if /I "%~1"=="quiet" (
  set "_LEON_QUIET=1"
  shift
  goto ParseArgs
)
if /I "%~1"=="optional" (
  set "_LEON_OPTIONAL=1"
  shift
  goto ParseArgs
)
if /I "%~1"=="need-ninja" (
  set "_LEON_NEED_NINJA=1"
  shift
  goto ParseArgs
)
if /I "%~1"=="need-git" (
  set "_LEON_NEED_GIT=1"
  shift
  goto ParseArgs
)
echo ERROR: _vsenv.bat unknown arg: %~1
echo   Usage: call Scripts\_vsenv.bat vcvars^|vsdev [quiet] [optional] [need-ninja] [need-git]
exit /b 1

:Parsed
if not defined _LEON_MODE (
  echo ERROR: _vsenv.bat needs mode vcvars or vsdev
  exit /b 1
)

set "PATH=C:\Program Files\CMake\bin;%PATH%"

if /I "%_LEON_MODE%"=="vcvars" goto DoVcvars
goto DoVsdev

:DoVcvars
set "_LEON_TOOL="
for %%E in (Community Professional Enterprise) do (
  if not defined _LEON_TOOL if exist "%ProgramFiles%\Microsoft Visual Studio\18\%%E\VC\Auxiliary\Build\vcvars64.bat" (
    set "_LEON_TOOL=%ProgramFiles%\Microsoft Visual Studio\18\%%E\VC\Auxiliary\Build\vcvars64.bat"
  )
)
for %%E in (Community Professional Enterprise) do (
  if not defined _LEON_TOOL if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
    set "_LEON_TOOL=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
  )
)
if not defined _LEON_TOOL (
  if "%_LEON_OPTIONAL%"=="1" exit /b 0
  echo ERROR: vcvars64.bat not found. See Docs/SETUP.md
  exit /b 1
)
if "%_LEON_QUIET%"=="1" (
  call "%_LEON_TOOL%" >nul
) else (
  call "%_LEON_TOOL%"
)
if errorlevel 1 exit /b 1
goto AfterEnv

:DoVsdev
set "_LEON_TOOL="
for %%E in (Community Professional Enterprise) do (
  if not defined _LEON_TOOL if exist "%ProgramFiles%\Microsoft Visual Studio\18\%%E\Common7\Tools\VsDevCmd.bat" (
    set "_LEON_TOOL=%ProgramFiles%\Microsoft Visual Studio\18\%%E\Common7\Tools\VsDevCmd.bat"
  )
)
for %%E in (Community Professional Enterprise) do (
  if not defined _LEON_TOOL if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\Common7\Tools\VsDevCmd.bat" (
    set "_LEON_TOOL=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\Common7\Tools\VsDevCmd.bat"
  )
)
if not defined _LEON_TOOL (
  if "%_LEON_OPTIONAL%"=="1" exit /b 0
  echo ERROR: VsDevCmd.bat not found. See Docs/SETUP.md
  exit /b 1
)
if "%_LEON_QUIET%"=="1" (
  call "%_LEON_TOOL%" -arch=amd64 -host_arch=amd64 >nul
) else (
  call "%_LEON_TOOL%" -arch=amd64 -host_arch=amd64
)
if errorlevel 1 exit /b 1

:AfterEnv
where cmake >nul 2>&1 || (
  echo ERROR: cmake not found
  exit /b 1
)
if "%_LEON_NEED_NINJA%"=="1" (
  where ninja >nul 2>&1 || (
    echo ERROR: Ninja not on PATH — winget install Ninja-build.Ninja
    exit /b 1
  )
)
if "%_LEON_NEED_GIT%"=="1" (
  where git >nul 2>&1 || (
    echo ERROR: git not found
    exit /b 1
  )
)
exit /b 0
