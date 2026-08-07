@echo off
REM Scripts\package-editor.bat — portable Editor + SDK into Dist\LeonEditor\
REM Copy that folder to another PC to run the editor; Build Game needs VS + CMake there.
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."

set "ROOT=%CD%"
set "OUT=%ROOT%\Dist\LeonEditor"

REM Optional: VS env helps if we need to call build-fast below.
call "%~dp0_vsenv.bat" vsdev quiet optional

REM Prefer day-to-day Ninja editor; fall back to VS Release.
set "EDITOR_EXE="
if exist "%ROOT%\Editor\build-fast\LeonEngine.exe" set "EDITOR_EXE=%ROOT%\Editor\build-fast\LeonEngine.exe"
if not defined EDITOR_EXE if exist "%ROOT%\Editor\build\Release\LeonEngine.exe" (
  set "EDITOR_EXE=%ROOT%\Editor\build\Release\LeonEngine.exe"
)
REM Legacy name during transition.
if not defined EDITOR_EXE if exist "%ROOT%\Editor\build-fast\leon-editor.exe" (
  set "EDITOR_EXE=%ROOT%\Editor\build-fast\leon-editor.exe"
)
if not defined EDITOR_EXE if exist "%ROOT%\Editor\build\Release\leon-editor.exe" (
  set "EDITOR_EXE=%ROOT%\Editor\build\Release\leon-editor.exe"
)
if not defined EDITOR_EXE (
  echo Building editor ^(build-fast^) ...
  call "%ROOT%\Scripts\build-fast.bat"
  if errorlevel 1 exit /b 1
  set "EDITOR_EXE=%ROOT%\Editor\build-fast\LeonEngine.exe"
)
if not exist "%EDITOR_EXE%" (
  echo ERROR: LeonEngine.exe not found
  exit /b 1
)

for %%I in ("%EDITOR_EXE%") do set "EDITOR_DIR=%%~dpI"

echo Packaging portable Editor + SDK
echo   from: %EDITOR_EXE%
echo   to:   %OUT%

REM Unique keep-dir so concurrent package runs cannot clobber each other.
set "KEEP_PROJECTS=%TEMP%\LeonEditor_Projects_Keep_%RANDOM%_%RANDOM%"
if exist "%KEEP_PROJECTS%" rmdir /s /q "%KEEP_PROJECTS%"
if exist "%OUT%\Projects" (
  mkdir "%KEEP_PROJECTS%" 2>nul
  call :RoboOptional "%OUT%\Projects" "%KEEP_PROJECTS%" || exit /b 1
)

if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%" 2>nul

copy /Y "%EDITOR_EXE%" "%OUT%\LeonEngine.exe" >nul
if errorlevel 1 exit /b 1

REM Runtime data staged beside the editor build.
call :RoboRequired "%EDITOR_DIR%assets" "%OUT%\assets" || exit /b 1
if exist "%EDITOR_DIR%Templates" call :RoboOptional "%EDITOR_DIR%Templates" "%OUT%\Templates" || exit /b 1
if exist "%EDITOR_DIR%Projects" call :RoboOptional "%EDITOR_DIR%Projects" "%OUT%\Projects" || exit /b 1
REM Also pick up projects staged under VS Release (often where New Project lands).
if exist "%ROOT%\Editor\build\Release\Projects" (
  call :RoboOptional "%ROOT%\Editor\build\Release\Projects" "%OUT%\Projects" || exit /b 1
)

REM SDK sources so Build → Build Game works on the other machine.
call :RoboRequired "%ROOT%\Engine" "%OUT%\Engine" || exit /b 1
call :RoboRequired "%ROOT%\Runtime" "%OUT%\Runtime" || exit /b 1
call :RoboRequired "%ROOT%\Plugins" "%OUT%\Plugins" || exit /b 1
call :RoboRequired "%ROOT%\Build" "%OUT%\Build" || exit /b 1
call :RoboRequired "%ROOT%\ThirdParty" "%OUT%\ThirdParty" || exit /b 1
if exist "%ROOT%\Templates" call :RoboOptional "%ROOT%\Templates" "%OUT%\Templates" || exit /b 1

mkdir "%OUT%\Scripts" 2>nul
copy /Y "%ROOT%\Scripts\_vsenv.bat" "%OUT%\Scripts\_vsenv.bat" >nul
copy /Y "%ROOT%\Scripts\build-project.bat" "%OUT%\Scripts\build-project.bat" >nul
copy /Y "%ROOT%\Scripts\package-editor.bat" "%OUT%\Scripts\package-editor.bat" >nul
if exist "%ROOT%\Scripts\cook.bat" copy /Y "%ROOT%\Scripts\cook.bat" "%OUT%\Scripts\cook.bat" >nul

if not exist "%OUT%\Projects" mkdir "%OUT%\Projects"
if exist "%KEEP_PROJECTS%" (
  call :RoboOptional "%KEEP_PROJECTS%" "%OUT%\Projects" || exit /b 1
  rmdir /s /q "%KEEP_PROJECTS%"
)

(
echo Leon Engine — portable editor
echo =============================
echo.
echo Run:  LeonEngine.exe
echo.
echo This folder is self-contained for EDITING ^(engine is linked into the exe^).
echo.
echo Build → Build Game ^(compile a game^) also needs on the target PC:
echo   - Visual Studio 2022/2026 with C++ workload
echo   - CMake
echo   - Internet the first time ^(FetchContent: glfw, glm, ...^)
echo.
echo After Build → Build Game, a clean game folder is written to:
echo   Projects\^<Name^>\Shipping\   ^(or your project folder\Shipping\^)
echo Copy THAT Shipping folder to ship/play the game.
echo.
echo VC++ Redistributable may be required:
echo   https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist
) > "%OUT%\README.txt"

echo.
echo Done: %OUT%
echo Copy the whole LeonEditor folder to another PC.
echo Run:  %OUT%\LeonEngine.exe
exit /b 0

:RoboRequired
if not exist "%~1" (
  echo ERROR: missing required source: %~1
  exit /b 1
)
call :Robo "%~1" "%~2"
exit /b !ERRORLEVEL!

:RoboOptional
if not exist "%~1" (
  echo SKIP missing optional: %~1
  exit /b 0
)
call :Robo "%~1" "%~2"
exit /b !ERRORLEVEL!

:Robo
REM robocopy: 0-7 = success
robocopy "%~1" "%~2" /E /NFL /NDL /NJH /NJS /nc /ns /np ^
  /XD build build-fast build-ninja build-linux .git _deps CMakeFiles ^
  /XF *.obj *.pdb *.ilk *.exp .DS_Store >nul
set "RC=!ERRORLEVEL!"
if !RC! GEQ 8 (
  echo ERROR: robocopy failed %~1 -^> %~2 ^(code !RC!^)
  exit /b 1
)
exit /b 0
