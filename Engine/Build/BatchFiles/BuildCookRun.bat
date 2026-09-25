@echo off
REM Engine\Build\BatchFiles\BuildCookRun.bat -project=<Project>.lproj -platform=Win64 [-configuration=Shipping|Development]
REM     [-build] [-cook] [-stage] [-pak] [-run] [-addcmdline="<game arguments>"] [-align=<bytes>]
REM Builds, cooks, stages and paks a project into <Project>\Saved\StagedBuilds\<Platform>\ and runs it (UE: RunUAT
REM BuildCookRun). The steps are in BuildCookRun.ps1.
setlocal EnableExtensions
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0BuildCookRun.ps1" %*
exit /b %ERRORLEVEL%
