@echo off
REM Engine\Build\BatchFiles\Cook.bat <LeonCook arguments>
REM   Cook.bat staticmesh --obj Mesh.obj --out Content\Meshes\Mesh.lmesh
REM   Cook.bat recipe Content\CookRecipe.json
REM Builds LeonCook (Win64 Development) and runs the cook commandlet (UE: UE4Editor-Cmd -run=cook).
setlocal EnableExtensions
set "LEON_ROOT=%~dp0..\..\.."
call "%~dp0Build.bat" LeonCook Win64 Development
if errorlevel 1 exit /b 1
"%LEON_ROOT%\Engine\Binaries\Win64\LeonCook.exe" %*
exit /b %ERRORLEVEL%
