@echo off
REM Engine\Build\BatchFiles\Rebuild.bat <Target> <Platform> <Configuration> [-Project=<file.lproj>]
REM Clean + Build (UE: Rebuild.bat).
setlocal EnableExtensions
call "%~dp0Build.bat" %* -Mode=Rebuild
exit /b %ERRORLEVEL%
