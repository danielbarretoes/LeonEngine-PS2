@echo off
REM Engine\Build\BatchFiles\Lint.bat — clang-format check, banned-API check (G4) + /W4 build of every Win64 engine target
REM and of ShooterGame's targets.
setlocal EnableExtensions

call "%~dp0FormatCode.bat" --check
if errorlevel 1 (
  echo Format check failed. Run Engine\Build\BatchFiles\FormatCode.bat
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0CheckBannedApis.ps1"
if errorlevel 1 exit /b 1

for %%T in (LeonAutomationTests LeonCook LeonPak LeonGame BlankProgram) do (
  call "%~dp0Build.bat" %%T Win64 Development
  if errorlevel 1 exit /b 1
)
REM The code project's targets: the game and its test program (Game\ShooterGame).
for %%T in (ShooterGame ShooterGameTests) do (
  call "%~dp0Build.bat" %%T Win64 Development "-Project=%~dp0..\..\..\Game\ShooterGame\ShooterGame.lproj"
  if errorlevel 1 exit /b 1
)
echo Lint OK
endlocal
