@echo off
REM Scripts\smoke-coop-dedicated.bat — headless CoopTp --dedicated must reach Courtyard.
setlocal EnableExtensions
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0smoke-coop-dedicated.ps1" %*
exit /b %ERRORLEVEL%
