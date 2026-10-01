@echo off
REM Engine\Build\BatchFiles\BotMatch.bat [Rounds] [Seed] [Map]
REM The headless bot match (plan P21): builds ShooterGame (Win64 Development), plays de_leon (or Map, a long package
REM name such as /Game/Maps/de_puerto, passed as -map=) with ten bots and the local player spectating (-botmatch), at
REM the fixed 30 Hz steps as fast as it can (-nullrhi -benchmark), for Rounds rounds (10) with the round stream's Seed
REM (7). Every frame the game checks the match's invariants
REM (FShooterMatchChecker: the round ends and the scores, the money, the team sizes, the pawns' health and floor);
REM it exits 1 when one broke or the rounds did not end in time. It fails when the exit code is not 0 or the game's
REM "Botmatch OK" line is missing. The match is played twice: the same seed must give the same summary line (the
REM rounds' reasons, the score, the kills), so a nondeterministic change fails too. The logs are
REM Game\ShooterGame\Saved\Logs\BotMatch[_<Map>].log and BotMatchReplay[_<Map>].log.
setlocal EnableExtensions EnableDelayedExpansion
set "LEON_ROOT=%~dp0..\..\.."
set "PROJECT=%LEON_ROOT%\Game\ShooterGame\ShooterGame.lproj"
set "ROUNDS=%~1"
if "%ROUNDS%"=="" set "ROUNDS=10"
set "SEED=%~2"
if "%SEED%"=="" set "SEED=7"
set "MAP_ARG="
set "MAP_NAME=de_leon"
set "LOG_SUFFIX="
if not "%~3"=="" (
  set "MAP_ARG=-map=%~3"
  set "MAP_NAME=%~n3"
  set "LOG_SUFFIX=_%~n3"
)
call "%~dp0Build.bat" ShooterGame Win64 Development "-Project=%PROJECT%"
if errorlevel 1 exit /b 1
pushd "%LEON_ROOT%"
set "MATCH_LOG=Game\ShooterGame\Saved\Logs\BotMatch%LOG_SUFFIX%.log"
if not exist "Game\ShooterGame\Saved\Logs" mkdir "Game\ShooterGame\Saved\Logs"
"Game\ShooterGame\Binaries\Win64\ShooterGame.exe" %MAP_ARG% -nullrhi -benchmark -botmatch -rounds=%ROUNDS% -seed=%SEED% > "%MATCH_LOG%" 2>&1
set "GAME_EXIT=%ERRORLEVEL%"
findstr /c:"Round " /c:"Botmatch" "%MATCH_LOG%"
if not "%GAME_EXIT%"=="0" (
  echo BotMatch FAILED: ShooterGame exited with %GAME_EXIT% ^(log: %MATCH_LOG%^)
  popd
  exit /b 1
)
set "FIRST="
for /f "delims=" %%L in ('findstr /l /c:"Botmatch OK" "%MATCH_LOG%"') do set "FIRST=%%L"
if not defined FIRST (
  echo BotMatch FAILED: no "Botmatch OK" ^(log: %MATCH_LOG%^)
  popd
  exit /b 1
)
set "REPLAY_LOG=Game\ShooterGame\Saved\Logs\BotMatchReplay%LOG_SUFFIX%.log"
"Game\ShooterGame\Binaries\Win64\ShooterGame.exe" %MAP_ARG% -nullrhi -benchmark -botmatch -rounds=%ROUNDS% -seed=%SEED% > "%REPLAY_LOG%" 2>&1
set "SECOND="
for /f "delims=" %%L in ('findstr /l /c:"Botmatch OK" "%REPLAY_LOG%"') do set "SECOND=%%L"
if not "!FIRST!"=="!SECOND!" (
  echo BotMatch FAILED: the replay with seed %SEED% differs ^(logs: %MATCH_LOG%, %REPLAY_LOG%^)
  echo   first:  !FIRST!
  echo   replay: !SECOND!
  popd
  exit /b 1
)
popd
echo BotMatch OK: %MAP_NAME%, %ROUNDS% round^(s^), seed %SEED%, exit code 0, replayed identically
exit /b 0
