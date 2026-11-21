@echo off
rem P07 SquadQuery - run the automation tests headless in the editor and exit with their result.
rem Usage: run_tests.bat [Filter]   (default filter: SquadQuery, i.e. every test of this project)
rem Exit code 0 = all passed; non-zero = a failure (AutomationCommandline.cpp prints "TEST COMPLETE. EXIT CODE").
rem Needs the Development editor target: build.bat SquadQueryEditor Development
setlocal
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=%~dp0..\SquadQuery\SquadQuery.uproject"
set "FILTER=%~1"
if "%FILTER%"=="" set "FILTER=SquadQuery"
set "REPORT=%~dp0..\SquadQuery\Saved\Automation\Reports"
set "LOG=%~dp0..\SquadQuery\Saved\Logs\Tests.log"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -ExecCmds="Automation RunTests %FILTER%;Quit" -unattended -nullrhi -nosound -nosplash -nopause -log -abslog="%LOG%" -ReportExportPath="%REPORT%"
exit /b %ERRORLEVEL%
