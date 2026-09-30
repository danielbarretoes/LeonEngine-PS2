@echo off
REM Engine\Build\BatchFiles\SmokeTest.bat
REM Gate G6, the ShooterGame smoke: builds ShooterGame (Win64 Development), boots it headless straight into a match on
REM de_leon past the main menu (the URL /Game/Maps/de_leon?team=CT: the team menu's choice), where the nine bots join
REM around the player (five a side with the local player: AShooterGameMode::RebalanceBots), runs the frames and exits. It fails when the game's exit code is not 0 or when the game mode's end-of-match line
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
"Game\ShooterGame\Binaries\Win64\ShooterGame.exe" "/Game/Maps/de_leon?team=CT" -nullrhi -ExitAfterFrames=120 > "%SMOKE_LOG%" 2>&1
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
