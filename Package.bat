@echo off
REM Package.bat - build and package each project under its own Packages\ folder.
REM     Package.bat [-NoWin64] [-NoPS2]
REM Win64: ShooterGame Shipping -> Game\ShooterGame\Packages\Win64\
REM PS2:   Game\<Name>\Packages\PS2\ (games projects); Engine\Packages\PS2\<Name>\ (engine programs)
REM        via Publish-Package (Docs/BUILD.md#ps2-staging-matrix).
REM The steps are in Engine\Build\BatchFiles\Package.ps1.
setlocal EnableExtensions
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Engine\Build\BatchFiles\Package.ps1" %*
set PACKAGE_EXIT=%ERRORLEVEL%
if not defined CI pause
exit /b %PACKAGE_EXIT%
