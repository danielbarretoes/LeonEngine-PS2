@echo off
REM Scripts\build-project.bat — cmake-build a Leon game pack into the project folder.
REM Intermediate:  <project>\build-fast\  (or build\Release)
REM Portable ship: <project>\Shipping\   (copy this folder to another PC)
REM Usage:
REM   Scripts\build-project.bat <projectDir> [cmakeTarget] [--with-server]
REM   Scripts\build-project.bat Projects\CoopTp
REM   Scripts\build-project.bat Projects\CoopTp leon-CoopTp --with-server
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."
set "ROOT=%CD%"

if "%~1"=="" (
  echo Usage: Scripts\build-project.bat ^<projectDir^> [cmakeTarget] [--with-server]
  echo   Scripts\build-project.bat Projects\Smoke leon-smoke
  echo   Scripts\build-project.bat Projects\CoopTp leon-CoopTp --with-server
  echo LEON_BUILD_EXIT=1
  exit /b 1
)

set "PROJ=%~f1"
if not exist "%PROJ%\CMakeLists.txt" (
  echo ERROR: No CMakeLists.txt in "%PROJ%"
  echo LEON_BUILD_EXIT=1
  exit /b 1
)

for %%I in ("%PROJ%") do set "PROJ_NAME=%%~nxI"

set "TARGET="
set "WITH_SERVER=0"
if not "%~2"=="" (
  if /I "%~2"=="--with-server" (
    set "WITH_SERVER=1"
  ) else (
    set "TARGET=%~2"
  )
)
if not "%~3"=="" (
  if /I "%~3"=="--with-server" set "WITH_SERVER=1"
)
if "%TARGET%"=="" set "TARGET=leon-%PROJ_NAME%"
set "SERVER_TARGET=leon-%PROJ_NAME%-server"

call "%~dp0_vsenv.bat" vsdev quiet
if errorlevel 1 (
  echo LEON_BUILD_EXIT=1
  exit /b 1
)
cd /d "%ROOT%"

echo Building game project: %PROJ%
echo CMake target: %TARGET%
if "%WITH_SERVER%"=="1" echo Also building: %SERVER_TARGET%
echo Leon root: %ROOT%

set "BUILD_DIR="
set "EXE_PATH="
set "SERVER_EXE_PATH="

where ninja >nul 2>&1
if errorlevel 1 goto VsBuild

echo Configuring "%PROJ%\build-fast" ^(Ninja / Release^) ...
cmake -S "%PROJ%" -B "%PROJ%\build-fast" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLEON_REPO_ROOT="%ROOT%" -DLEON_ENGINE_ASSETS="%ROOT%\Engine\Assets" -DLEON_PROJECTS_ROOT="%ROOT%\Projects"
if errorlevel 1 (
  echo LEON_BUILD_EXIT=1
  exit /b 1
)
echo Building %TARGET%...
cmake --build "%PROJ%\build-fast" --target "%TARGET%"
if errorlevel 1 (
  echo LEON_BUILD_EXIT=1
  exit /b 1
)
if "%WITH_SERVER%"=="1" (
  echo Building %SERVER_TARGET%...
  cmake --build "%PROJ%\build-fast" --target "%SERVER_TARGET%"
  if errorlevel 1 (
    echo LEON_BUILD_EXIT=1
    exit /b 1
  )
  set "SERVER_EXE_PATH=%PROJ%\build-fast\%SERVER_TARGET%.exe"
)
set "BUILD_DIR=%PROJ%\build-fast"
set "EXE_PATH=%PROJ%\build-fast\%TARGET%.exe"
goto Export

:VsBuild
echo Configuring "%PROJ%\build" ^(Visual Studio^) ...
cmake -S "%PROJ%" -B "%PROJ%\build" -G "Visual Studio 18 2026" -A x64 -DLEON_REPO_ROOT="%ROOT%" -DLEON_ENGINE_ASSETS="%ROOT%\Engine\Assets" -DLEON_PROJECTS_ROOT="%ROOT%\Projects" 2>nul
if errorlevel 1 (
  cmake -S "%PROJ%" -B "%PROJ%\build" -G "Visual Studio 17 2022" -A x64 -DLEON_REPO_ROOT="%ROOT%" -DLEON_ENGINE_ASSETS="%ROOT%\Engine\Assets" -DLEON_PROJECTS_ROOT="%ROOT%\Projects"
  if errorlevel 1 (
    echo LEON_BUILD_EXIT=1
    exit /b 1
  )
)
echo Building %TARGET%...
cmake --build "%PROJ%\build" --config Release --target "%TARGET%"
if errorlevel 1 (
  echo LEON_BUILD_EXIT=1
  exit /b 1
)
if "%WITH_SERVER%"=="1" (
  echo Building %SERVER_TARGET%...
  cmake --build "%PROJ%\build" --config Release --target "%SERVER_TARGET%"
  if errorlevel 1 (
    echo LEON_BUILD_EXIT=1
    exit /b 1
  )
  set "SERVER_EXE_PATH=%PROJ%\build\Release\%SERVER_TARGET%.exe"
)
set "BUILD_DIR=%PROJ%\build\Release"
set "EXE_PATH=%PROJ%\build\Release\%TARGET%.exe"

:Export
if not exist "%EXE_PATH%" (
  echo ERROR: expected exe missing: %EXE_PATH%
  echo LEON_BUILD_EXIT=1
  exit /b 1
)
if "%WITH_SERVER%"=="1" if not exist "%SERVER_EXE_PATH%" (
  echo ERROR: expected server exe missing: %SERVER_EXE_PATH%
  echo LEON_BUILD_EXIT=1
  exit /b 1
)

REM Portable game next to the project (not Dist\ at repo root).
set "SHIP=%PROJ%\Shipping"
echo.
echo Exporting portable game into the project folder:
echo   %SHIP%
if exist "%SHIP%" rmdir /s /q "%SHIP%"
mkdir "%SHIP%" 2>nul

copy /Y "%EXE_PATH%" "%SHIP%\%TARGET%.exe" >nul
if "%WITH_SERVER%"=="1" (
  copy /Y "%SERVER_EXE_PATH%" "%SHIP%\%SERVER_TARGET%.exe" >nul
  (
    echo @echo off
    echo REM Headless dedicated server — default port 7777
    echo "%%~dp0%SERVER_TARGET%.exe" --dedicated --port 7777 --tick 60 %%*
  ) > "%SHIP%\run-dedicated.bat"
)
if exist "%BUILD_DIR%\assets" (
  robocopy "%BUILD_DIR%\assets" "%SHIP%\assets" /E /NFL /NDL /NJH /NJS /nc /ns /np >nul
)
if exist "%BUILD_DIR%\Projects" (
  robocopy "%BUILD_DIR%\Projects" "%SHIP%\Projects" /E /NFL /NDL /NJH /NJS /nc /ns /np >nul
)

(
echo %PROJ_NAME% — Leon game ^(portable^)
echo.
echo Run client:  %TARGET%.exe
if "%WITH_SERVER%"=="1" (
  echo Run dedicated:  %SERVER_TARGET%.exe
  echo   or double-click run-dedicated.bat
  echo Clients: Join Dedicated from Main Menu ^(same LAN IP:7777^)
)
echo This folder is inside your project: Shipping\
echo Copy this entire Shipping folder to another PC to play.
echo VC++ Redistributable may be required.
echo.
echo Linux: build natively with Scripts/build-project.sh ^(no Windows cross-compile^).
) > "%SHIP%\README.txt"

if not exist "%SHIP%\%TARGET%.exe" (
  echo ERROR: Shipping export failed: %SHIP%\%TARGET%.exe
  echo LEON_BUILD_EXIT=1
  exit /b 1
)

echo.
echo ========================================
echo Build OK
echo   Intermediate: %EXE_PATH%
if "%WITH_SERVER%"=="1" echo   Server:       %SERVER_EXE_PATH%
echo   Ship / play:  %SHIP%\%TARGET%.exe
if "%WITH_SERVER%"=="1" echo   Ship server:  %SHIP%\%SERVER_TARGET%.exe
echo ========================================
echo LEON_BUILD_EXIT=0
exit /b 0
