@echo off
rem Builds ScriptingTestEditor and runs the Bango Triggers automation tests (Triggers.*).
rem   BuildBangoTriggers.bat          build, then run tests
rem   BuildBangoTriggers.bat build    build only
rem   BuildBangoTriggers.bat test     tests only
rem Full output goes to <project>\Saved\BangoTriggersBuild\build.log and tests.log; status.txt says how it ended.

setlocal
set "UE=X:\UnrealEngine\UE_5.8"
rem This file is in <project>\Plugins\BangoTriggers.
for %%I in ("%~dp0..\..") do set "PROJDIR=%%~fI"
set "PROJ=%PROJDIR%\ScriptingTest.uproject"
set "LOGDIR=%PROJDIR%\Saved\BangoTriggersBuild"
if not exist "%LOGDIR%" mkdir "%LOGDIR%"
echo RUNNING %DATE% %TIME% > "%LOGDIR%\status.txt"

if /i "%~1"=="test" goto :tests

echo === Building ScriptingTestEditor (Win64 Development)...
call "%UE%\Engine\Build\BatchFiles\Build.bat" ScriptingTestEditor Win64 Development -Project="%PROJ%" -WaitMutex -NoHotReloadFromIDE > "%LOGDIR%\build.log" 2>&1
set BUILD_RESULT=%ERRORLEVEL%
findstr /r /c:": error" /c:": warning" /c:"Result: " /c:"Total execution time" "%LOGDIR%\build.log"
if not "%BUILD_RESULT%"=="0" (
	echo === BUILD FAILED ^(exit code %BUILD_RESULT%^). See Saved\BangoTriggersBuild\build.log
	echo BUILD FAILED %BUILD_RESULT% > "%LOGDIR%\status.txt"
	pause
	exit /b %BUILD_RESULT%
)
echo === BUILD SUCCEEDED
if /i "%~1"=="build" (
	echo BUILD SUCCEEDED > "%LOGDIR%\status.txt"
	pause
	exit /b 0
)

:tests
echo === Running Bango Triggers automation tests...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -ExecCmds="Automation RunTests StartsWith:Triggers." -TestExit="Automation Test Queue Empty" -unattended -nullrhi -nosplash -nosound -nopause -log -stdout -FullStdOutLogOutput > "%LOGDIR%\tests.log" 2>&1
set TEST_RESULT=%ERRORLEVEL%
findstr /c:"Test Completed" /c:"Error:" /c:"**** TEST COMPLETE" "%LOGDIR%\tests.log" | findstr /i "Triggers Result= TEST Error"
rem The editor can exit with 0 even when a test fails, so also look for failed results in the log.
if "%TEST_RESULT%"=="0" (
	findstr /c:"Result={Fail" "%LOGDIR%\tests.log" > nul && set TEST_RESULT=1
)
if "%TEST_RESULT%"=="0" (
	findstr /c:"Result={Success" "%LOGDIR%\tests.log" > nul || set TEST_RESULT=2
)
if not "%TEST_RESULT%"=="0" (
	echo === TESTS FAILED OR DID NOT RUN ^(exit code %TEST_RESULT%^). See Saved\BangoTriggersBuild\tests.log
	echo TESTS FAILED %TEST_RESULT% > "%LOGDIR%\status.txt"
	pause
	exit /b %TEST_RESULT%
)
echo === TESTS FINISHED. See Saved\BangoTriggersBuild\tests.log
echo TESTS PASSED > "%LOGDIR%\status.txt"

pause

exit /b 0
