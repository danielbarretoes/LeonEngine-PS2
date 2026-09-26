@echo off
REM Engine\Build\BatchFiles\GetVSEnv.bat - the MSVC environment and CMake on PATH for Leon's batch files (Build.bat,
REM GenerateProjectFiles.bat): finds Visual Studio 18 then 2022 (Community, Professional, Enterprise), runs its
REM vcvars64.bat quietly, and checks that CMake and Ninja are on PATH.
REM   call "%~dp0GetVSEnv.bat"

set "PATH=C:\Program Files\CMake\bin;%PATH%"

set "_LEON_TOOL="
for %%V in (18 2022) do for %%E in (Community Professional Enterprise) do (
  if not defined _LEON_TOOL if exist "%ProgramFiles%\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" (
    set "_LEON_TOOL=%ProgramFiles%\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat"
  )
)
if not defined _LEON_TOOL (
  echo ERROR: vcvars64.bat not found. See Docs/SETUP.md
  exit /b 1
)
call "%_LEON_TOOL%" >nul
if errorlevel 1 exit /b 1

where cmake >nul 2>&1 || (
  echo ERROR: cmake not found
  exit /b 1
)
where ninja >nul 2>&1 || (
  echo ERROR: Ninja not on PATH - winget install Ninja-build.Ninja
  exit /b 1
)
exit /b 0
