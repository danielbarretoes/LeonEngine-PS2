@echo off
REM Engine\Build\BatchFiles\Build.bat <Target> <Platform> <Configuration> [-Project=<file.leonproject>] [-Mode=...]
REM   Build.bat BlankProgram Win64 Development
REM   Build.bat ThirdPerson PS2 Development -Project=%CD%\Game\ThirdPerson\ThirdPerson.leonproject
REM Runs LeonBuildTool (CMake script mode). Win64 needs the MSVC environment; PS2 builds run in Docker.
setlocal EnableExtensions
set "LEON_ROOT=%~dp0..\..\.."

if /I not "%~2"=="PS2" (
  call "%~dp0GetVSEnv.bat" vcvars quiet need-ninja
  if errorlevel 1 exit /b 1
)

cmake -P "%LEON_ROOT%\Engine\Source\Programs\LeonBuildTool\LeonBuildTool.cmake" %*
exit /b %ERRORLEVEL%
