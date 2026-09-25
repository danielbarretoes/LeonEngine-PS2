@echo off
REM Engine\Build\BatchFiles\Cook.bat <LeonCook arguments>
REM   Cook.bat -run=ImportAssets -source=SourceArt\Mesh.obj -dest=/Game/Meshes
REM   Cook.bat Game\MyGame\MyGame.lproj -run=Cook
REM Builds LeonCook (Win64 Development) and runs it: a commandlet by name (UE: UE4Editor-Cmd <Project> -run=<Commandlet>).
setlocal EnableExtensions
set "LEON_ROOT=%~dp0..\..\.."
call "%~dp0Build.bat" LeonCook Win64 Development
if errorlevel 1 exit /b 1
"%LEON_ROOT%\Engine\Binaries\Win64\LeonCook.exe" %*
exit /b %ERRORLEVEL%
