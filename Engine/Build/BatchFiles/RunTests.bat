@echo off
REM Engine\Build\BatchFiles\RunTests.bat [-automation=<filter>]
REM Builds LeonAutomationTests (Win64 Development) and runs every module's Private/Tests from the repo root, then the
REM LeonHeaderTool golden tests (Build.bat builds the host tool into Engine\Intermediate\Build\HostTools\Win64), then
REM ShooterGame's tests: the project's test program ShooterGameTests (Game\ShooterGame\Source\ShooterGameTests.Target.cmake)
REM runs Game\ShooterGame\Source\ShooterGame\Private\Tests with the project's config. The filter applies to both. Last,
REM TestPAL (Core, CoreUObject, Json, Projects and PakFile without the engine: the PS2's test program, run on Win64).
setlocal EnableExtensions
set "LEON_ROOT=%~dp0..\..\.."

call "%~dp0Build.bat" LeonAutomationTests Win64 Development
if errorlevel 1 exit /b 1

pushd "%LEON_ROOT%"
"Engine\Binaries\Win64\LeonAutomationTests.exe" %*
set "RESULT=%ERRORLEVEL%"
"Engine\Intermediate\Build\HostTools\Win64\LeonHeaderTool.exe" -Test "Engine\Source\Programs\LeonHeaderTool\Tests"
if errorlevel 1 set "RESULT=1"

call "%~dp0Build.bat" ShooterGameTests Win64 Development "-Project=%CD%\Game\ShooterGame\ShooterGame.lproj"
if errorlevel 1 (
  popd
  exit /b 1
)
"Game\ShooterGame\Binaries\Win64\ShooterGameTests.exe" %*
if errorlevel 1 set "RESULT=1"

call "%~dp0Build.bat" TestPAL Win64 Development
if errorlevel 1 (
  popd
  exit /b 1
)
"Engine\Binaries\Win64\TestPAL.exe"
if errorlevel 1 set "RESULT=1"
popd
exit /b %RESULT%
