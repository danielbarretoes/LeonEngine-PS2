@echo off
REM Engine\Build\BatchFiles\SmokeTest.bat
REM Gate G6, the ShooterGame smoke: builds ShooterGame (Win64 Development), boots it headless on its default map
REM (de_leon), fills both teams with bots on the first frame (-ExecCmds=bot_fill: five a side with the local player),
REM runs the frames and exits. It fails when the game's exit code is not 0 or when the game mode's end-of-match line
REM does not report the ten pawns (the local player's and nine bots') in their teams.
setlocal EnableExtensions
set "LEON_ROOT=%~dp0..\..\.."
set "PROJECT=%LEON_ROOT%\Game\ShooterGame\ShooterGame.lproj"
set "EXPECT=ShooterGameMode: 10 pawn(s) at the end of the match, CT 5, T 5"
call "%~dp0Build.bat" ShooterGame Win64 Development "-Project=%PROJECT%"
if errorlevel 1 exit /b 1
pushd "%LEON_ROOT%"
set "SMOKE_LOG=Game\ShooterGame\Saved\Logs\SmokeTest.log"
if not exist "Game\ShooterGame\Saved\Logs" mkdir "Game\ShooterGame\Saved\Logs"
"Game\ShooterGame\Binaries\Win64\ShooterGame.exe" -nullrhi -ExecCmds=bot_fill -ExitAfterFrames=120 > "%SMOKE_LOG%" 2>&1
set "GAME_EXIT=%ERRORLEVEL%"
findstr /c:"ShooterGame: " /c:"joined" /c:"ShooterGameMode:" "%SMOKE_LOG%"
if not "%GAME_EXIT%"=="0" (
  echo SmokeTest FAILED: ShooterGame exited with %GAME_EXIT% ^(log: %SMOKE_LOG%^)
  popd
  exit /b 1
)
findstr /l /c:"%EXPECT%" "%SMOKE_LOG%" >nul
if errorlevel 1 (
  echo SmokeTest FAILED: no "%EXPECT%" ^(log: %SMOKE_LOG%^)
  popd
  exit /b 1
)
popd
echo SmokeTest OK: 10 pawns, CT 5, T 5, exit code 0
exit /b 0
