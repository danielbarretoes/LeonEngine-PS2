@echo off
REM Scripts\cook.bat — leon-cook recipe ^<json^>
setlocal EnableExtensions
cd /d "%~dp0.."

if "%~1"=="" (
  echo Usage: Scripts\cook.bat ^<recipe.json^>
  echo   Scripts\cook.bat Templates\ThirdPerson\Content\assets\characters\bot\cook-bot.json
  exit /b 1
)

call "%~dp0_vsenv.bat" vsdev quiet
if errorlevel 1 exit /b 1

REM Prefer existing trees; configure Ninja if available, else VS.
if exist "Tools\build-ninja\leon-cook.exe" goto RunNinja
if exist "Tools\build\Release\leon-cook.exe" goto RunVs

where ninja >nul 2>&1
if not errorlevel 1 (
  echo Configuring Tools\build-ninja ...
  cmake -S Tools -B Tools/build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
  if errorlevel 1 exit /b 1
  goto RunNinja
)

echo Configuring Tools\build (Visual Studio) ...
cmake -S Tools -B Tools/build -G "Visual Studio 18 2026" -A x64 2>nul
if errorlevel 1 (
  cmake -S Tools -B Tools/build -G "Visual Studio 17 2022" -A x64
  if errorlevel 1 exit /b 1
)
goto RunVs

:RunNinja
cmake --build Tools/build-ninja --target leon-cook
if errorlevel 1 exit /b 1
Tools\build-ninja\leon-cook.exe recipe "%~1"
exit /b %ERRORLEVEL%

:RunVs
cmake --build Tools/build --config Release --target leon-cook
if errorlevel 1 exit /b 1
Tools\build\Release\leon-cook.exe recipe "%~1"
exit /b %ERRORLEVEL%
