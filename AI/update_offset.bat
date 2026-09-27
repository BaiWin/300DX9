@echo off
rem ============================================================================
rem  Generate offset_update.h.   DOUBLE-CLICK THIS FILE.
rem
rem  There is nothing to compile. No Visual Studio, no vcvars, no obj files.
rem  It just runs offset_update.py, and Python does all the work.
rem
rem  It ALWAYS scans. There is no "did the version change" check, on purpose:
rem  that check would skip the scan when you edited offsets_AI.h and the game
rem  version happened to be unchanged, and you would think your edit worked.
rem
rem  Output:
rem      AI\offset_update.h        the generated header, included by the DLL
rem
rem  IMPORTANT: this file must stay ASCII-only, no Chinese.
rem  cmd.exe parses .bat files with the OEM codepage (GBK on Chinese Windows),
rem  and UTF-8 Chinese bytes get mis-parsed and silently break the script.
rem  All the Chinese output comes from Python, not from here.
rem ============================================================================
setlocal
cd /d "%~dp0"

rem ---------------------------------------------------------------------------
rem  Find Python.
rem  "py" is the Windows launcher and is the most reliable. Fall back to the
rem  plain "python" name, which is what the official installer adds to PATH.
rem ---------------------------------------------------------------------------
set "PYTHON_COMMAND="

where py >nul 2>nul
if not errorlevel 1 set "PYTHON_COMMAND=py"

if not defined PYTHON_COMMAND (
    where python >nul 2>nul
    if not errorlevel 1 set "PYTHON_COMMAND=python"
)

if not defined PYTHON_COMMAND (
    echo [ERROR] Python not found in PATH.
    echo.
    echo         Install Python 3 from https://www.python.org/downloads/
    echo         and tick "Add python.exe to PATH" during setup.
    echo.
    pause
    exit /b 2
)

rem ---------------------------------------------------------------------------
rem  Run it. No arguments: the script always scans the whole table.
rem ---------------------------------------------------------------------------
rem  -B = do not write __pycache__. Keeps this directory clean; costs ~10ms.
"%PYTHON_COMMAND%" -B "%~dp0offset_update.py"
set "TOOL_EXIT_CODE=%ERRORLEVEL%"

echo.
if "%TOOL_EXIT_CODE%"=="0" goto :result_ok
if "%TOOL_EXIT_CODE%"=="1" goto :result_partial

echo ================================================================================
echo  [FAILED] See the messages above.
echo ================================================================================
goto :finish

:result_ok
echo ================================================================================
echo  [OK] offset_update.h is ready. Include it from the DLL.
echo ================================================================================
goto :finish

:result_partial
echo ================================================================================
echo  [WARN] offset_update.h was written, but some signatures did not resolve.
echo         The missing constants are listed at the top of offset_update.h.
echo         Fix those patterns in offsets_AI.h, then run this file again.
echo ================================================================================

:finish
echo.
rem  Keep the window open, otherwise a double-click closes it before you can
rem  read anything. Delete this line if you ever call this from a script.
pause
endlocal & exit /b %TOOL_EXIT_CODE%
