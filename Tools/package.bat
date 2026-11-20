@echo off
rem P07 SquadQuery - package a standalone Win64 build (BuildCookRun), the build every headline benchmark uses.
rem Usage: package.bat [Configuration]   Configuration: Development (default; headline benchmarks) | Shipping (no CSV profiler; not used)
rem Note: the launcher (installed) engine cannot build the Test configuration (09_Testing_And_Benchmarks.md, Phase 4).
rem Output: ..\Build\Windows\SquadQuery.exe   (Build\ is git-ignored)
rem Run it through the machine-wide queue (tools\ue_queue.py), never directly.
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\SquadQuery\SquadQuery.uproject"
set "ARCHIVE=%~dp0..\Build"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Development"

call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%PROJECT%" -noP4 -utf8output -unattended -platform=Win64 -clientconfig=%CONFIG% -build -cook -stage -pak -archive -archivedirectory="%ARCHIVE%"
exit /b %ERRORLEVEL%
