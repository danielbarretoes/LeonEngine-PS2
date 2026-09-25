@echo off
REM Engine\Build\BatchFiles\RunTests.bat [-automation=<filter>]
REM Builds LeonAutomationTests (Win64 Development) and runs every module's Private/Tests from the repo root.
setlocal EnableExtensions
set "LEON_ROOT=%~dp0..\..\.."

call "%~dp0Build.bat" LeonAutomationTests Win64 Development
if errorlevel 1 exit /b 1

pushd "%LEON_ROOT%"
"Engine\Binaries\Win64\LeonAutomationTests.exe" %*
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
