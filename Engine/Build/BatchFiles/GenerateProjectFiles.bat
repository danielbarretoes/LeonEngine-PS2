@echo off
REM Engine\Build\BatchFiles\GenerateProjectFiles.bat [-Project=<file.lproj>]
REM Visual Studio solution for browsing / debugging in <Engine|Project>\Intermediate\ProjectFiles, plus the
REM root compile_commands.json for clangd (UE: GenerateProjectFiles.bat). Builds keep using Build.bat (Ninja).
setlocal EnableExtensions
set "LEON_ROOT=%~dp0..\..\.."
call "%~dp0GetVSEnv.bat" vcvars quiet need-ninja
if errorlevel 1 exit /b 1
cmake -P "%LEON_ROOT%\Engine\Source\Programs\LeonBuildTool\LeonBuildTool.cmake" -- -Mode=GenerateProjectFiles %*
if errorlevel 1 exit /b 1
cmake -P "%LEON_ROOT%\Engine\Source\Programs\LeonBuildTool\LeonBuildTool.cmake" -- LeonAutomationTests Win64 Development -Mode=GenerateClangDatabase
exit /b %ERRORLEVEL%
