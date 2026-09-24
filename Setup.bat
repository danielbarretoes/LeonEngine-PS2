@echo off
REM Setup.bat — download the pinned third-party dependencies (UE: Setup.bat / GitDependencies).
setlocal EnableExtensions
cmake -P "%~dp0Engine\Source\Programs\LeonBuildTool\LeonBuildTool.cmake" -Mode=Setup
exit /b %ERRORLEVEL%
