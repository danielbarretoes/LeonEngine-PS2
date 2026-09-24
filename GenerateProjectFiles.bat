@echo off
REM GenerateProjectFiles.bat [-Project=<file.lproj>] (UE: root GenerateProjectFiles.bat)
call "%~dp0Engine\Build\BatchFiles\GenerateProjectFiles.bat" %*
exit /b %ERRORLEVEL%
