@echo off
REM Engine\Build\BatchFiles\CheckReimport.bat [<Project>.lproj ...]
REM Gate G5, reproducible reimport: reimports every asset of the engine content (and of each project given) from its
REM source art (LeonCook [<Project>.lproj] -run=ImportAssets -reimport -all), then fails when git sees a change under
REM a Content folder: the same sources must save the same bytes (deterministic saves and imports, no timestamps).
setlocal EnableExtensions
set "LEON_ROOT=%~dp0..\..\.."
call "%~dp0Build.bat" LeonCook Win64 Development
if errorlevel 1 exit /b 1
pushd "%LEON_ROOT%"
"Engine\Binaries\Win64\LeonCook.exe" -run=ImportAssets -reimport -all
if errorlevel 1 goto :Failed
:NextProject
if "%~1"=="" goto :Compare
"Engine\Binaries\Win64\LeonCook.exe" "%~f1" -run=ImportAssets -reimport -all
if errorlevel 1 goto :Failed
shift
goto :NextProject
:Compare
git diff --exit-code --stat -- Engine/Content "Game/*/Content/*"
if errorlevel 1 goto :Failed
REM A reimport must not make new files either.
for /f %%F in ('git ls-files --others --exclude-standard -- Engine/Content "Game/*/Content/*"') do goto :Failed
popd
echo CheckReimport OK
exit /b 0
:Failed
popd
echo CheckReimport FAILED: the reimport changed the content (see above)
exit /b 1
