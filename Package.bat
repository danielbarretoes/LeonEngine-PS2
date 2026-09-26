@echo off
REM Package.bat - build and package the project for Win64 and PS2 (double-click it, or run it from a console).
REM     Package.bat [-NoWin64] [-NoPS2]
REM Win64: ShooterGame, Shipping, cooked and staged with its pak (BuildCookRun) -> Packages\Win64\
REM PS2:   ThirdPerson and TestPAL, Development, with their staged config (Docker Desktop must be running) -> Packages\PS2\
REM The steps are in Engine\Build\BatchFiles\Package.ps1.
setlocal EnableExtensions
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Engine\Build\BatchFiles\Package.ps1" %*
set PACKAGE_EXIT=%ERRORLEVEL%
if not defined CI pause
exit /b %PACKAGE_EXIT%
