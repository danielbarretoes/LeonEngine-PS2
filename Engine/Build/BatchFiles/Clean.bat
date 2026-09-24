@echo off
REM Engine\Build\BatchFiles\Clean.bat <Target> <Platform> <Configuration> [-Project=<file.lproj>]
REM Removes the target's intermediate build tree (UE: Clean.bat).
setlocal EnableExtensions
call "%~dp0Build.bat" %* -Mode=Clean
exit /b %ERRORLEVEL%
