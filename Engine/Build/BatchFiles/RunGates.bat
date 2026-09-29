@echo off
REM Engine\Build\BatchFiles\RunGates.bat [-PS2] [-Measure]
REM Every local gate in order (Docs/TESTING.md; the repository has no CI: this is it), each one's result on one line and
REM a summary; exit code 1 when any failed. The logs go to Engine\Saved\Gates\<Gate>.log.
REM   G1 G4 /W4  Lint.bat (format, banned APIs, the warnings build)
REM   tests      RunTests.bat (LeonAutomationTests with G8's GS parity, the LeonHeaderTool golden tests,
REM              ShooterGameTests, TestPAL on Win64)
REM   G5         CheckReimport.bat with ShooterGame (needs a checkout without content changes)
REM   G6         SmokeTest.bat
REM   bot match  BotMatch.bat 10 7 (played twice, identically)
REM   content    LeonCook -run=ValidateAssets for the engine and ShooterGame
REM   -PS2       G3: Package.bat -NoWin64 (the PS2 artifacts in Docker; their ELF sizes go to Budgets.md by hand)
REM   -Measure   MeasurePS2.bat (PCSX2, unattended; prints the Budgets.md row)
setlocal EnableExtensions EnableDelayedExpansion
set "LEON_ROOT=%~dp0..\.."
set "LEON_ROOT=%LEON_ROOT%\.."
set "GATE_LOGS=%LEON_ROOT%\Engine\Saved\Gates"
if not exist "%GATE_LOGS%" mkdir "%GATE_LOGS%"
set "WITH_PS2="
set "WITH_MEASURE="
for %%A in (%*) do (
  if /i "%%~A"=="-PS2" set "WITH_PS2=1"
  if /i "%%~A"=="-Measure" set "WITH_MEASURE=1"
)
set "FAILED="
set "PROJECT=%LEON_ROOT%\Game\ShooterGame\ShooterGame.lproj"
REM Package.bat pauses at its end for a double-click unless CI is set.
set "CI=1"

set "GATE_CMD="%~dp0Lint.bat"" & call :Gate Lint
set "GATE_CMD="%~dp0RunTests.bat"" & call :Gate RunTests
set "GATE_CMD="%~dp0CheckReimport.bat" "%PROJECT%"" & call :Gate CheckReimport
set "GATE_CMD="%~dp0SmokeTest.bat"" & call :Gate SmokeTest
set "GATE_CMD="%~dp0BotMatch.bat" 10 7" & call :Gate BotMatch
set "GATE_CMD="%~dp0Cook.bat" -run=ValidateAssets" & call :Gate ValidateEngine
set "GATE_CMD="%~dp0Cook.bat" "%PROJECT%" -run=ValidateAssets" & call :Gate ValidateShooterGame
if defined WITH_PS2 set "GATE_CMD="%LEON_ROOT%\Package.bat" -NoWin64" & call :Gate PackagePS2
if defined WITH_MEASURE set "GATE_CMD="%~dp0MeasurePS2.bat"" & call :Gate MeasurePS2

echo.
if defined FAILED (
  echo RunGates FAILED:%FAILED%
  exit /b 1
)
echo RunGates OK
exit /b 0

REM :Gate <Name>: runs GATE_CMD (a whole command line: batch splits "-run=X" arguments at the "=") into its log and
REM records the result.
:Gate
call %GATE_CMD% > "%GATE_LOGS%\%~1.log" 2>&1
if errorlevel 1 (
  echo [FAIL] %~1 ^(log: Engine\Saved\Gates\%~1.log^)
  set "FAILED=!FAILED! %~1"
) else (
  echo [ OK ] %~1
)
exit /b 0
