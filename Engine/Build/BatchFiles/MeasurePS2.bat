@echo off
REM Engine\Build\BatchFiles\MeasurePS2.bat [-Project Game\ShooterGame] [-Rounds 2] [-Seed 7] [-Seconds 120]
REM                                         [-Map /Game/Maps/de_harbor] [-NoBuild]
REM The PS2 frame measured in PCSX2, unattended (Docs/PLANS/ps2-shipping.md N1): builds, cooks and stages the game,
REM runs a bot match watched through a bot's eyes with -LogFrameTimes, reads the EE log until its
REM "FrameStats Summary:" and "ProfileSummary:" lines, closes PCSX2, writes <Project>\Saved\Profiling\PS2Frame.csv and
REM prints a Budgets.md row and the frame's cycle stats ("Profile over N frames", N9).
REM Steps: Engine\Platforms\PS2\Build\BatchFiles\MeasurePS2.ps1.
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\..\Platforms\PS2\Build\BatchFiles\MeasurePS2.ps1" %*
exit /b %ERRORLEVEL%
