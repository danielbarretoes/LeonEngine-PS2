@echo off
REM Engine\Build\BatchFiles\Lint.bat — clang-format check, banned-API check (G4) + /W4 build of every Win64 engine target.
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
echo Lint OK
endlocal
