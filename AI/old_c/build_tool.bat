@echo off
rem ============================================================================
rem  Build signature_tool.exe, then RUN it.
rem  Output: AI\signature_tool.exe  and  AI\offset_update.h
rem
rem  Double-click this file. That is the whole workflow:
rem      1. compile the tool
rem      2. compile the DLL-side sources as objects (a compile check, not linked)
rem      3. run the tool with no arguments -> update flow
rem
rem  The update flow reads the game version, and ONLY rescans when the version
rem  changed. If nothing changed, offset_update.h is left untouched.
rem
rem  IMPORTANT: This file must stay ASCII-only, no Chinese.
rem  cmd.exe parses .bat files using the OEM codepage (GBK on Chinese Windows).
rem  UTF-8 Chinese bytes get mis-parsed and silently break the script.
rem  (This is the same encoding trap that bites C++ sources -- see README.)
rem
rem  Built as x86 (32-bit) on purpose: this engine will later live inside the
rem  injected DLL, and the game is 32-bit. x86 here guarantees that "the tool
rem  compiles" implies "the DLL will compile".
rem ============================================================================
rem  Delayed expansion, so that paths containing parentheses can be echoed from
rem  inside "if (...)" blocks. With normal percent-expansion, a value like
rem  "C:\Program Files (x86)\..." has its ")" close the enclosing block early,
rem  and the tail of the path gets parsed as a fresh command.
setlocal EnableDelayedExpansion

rem ---------------------------------------------------------------------------
rem  Already have a toolchain in PATH? Use it. Saves ~2s per build.
rem ---------------------------------------------------------------------------
where cl >nul 2>nul
if not errorlevel 1 goto :have_toolchain

rem ---------------------------------------------------------------------------
rem  Locate Visual Studio.
rem
rem  NOTE: do NOT write this as a "for /f ... in (...)" one-liner that runs
rem  vswhere inline. The ProgramFiles-x86 variable expands to a path containing
rem  parentheses, and cmd.exe closes the for-block at the ")" inside the path.
rem  The command gets truncated mid-path and the loop body never runs.
rem  A temp file sidesteps the parser entirely.
rem
rem  Also: keep this comment free of percent signs and apostrophes -- cmd.exe
rem  expands variables inside "rem" lines and treats the quote as unbalanced.
rem ---------------------------------------------------------------------------
set "PF86=%ProgramFiles(x86)%"
if not defined PF86 set "PF86=%ProgramFiles%"
set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "!VSWHERE!" (
    echo [ERROR] vswhere.exe not found at:
    echo         !VSWHERE!
    echo         Install Visual Studio 2022 with the C++ workload.
    exit /b 1
)

set "VSDIR="
"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\_signature_vsdir.txt" 2>nul
set /p VSDIR=<"%TEMP%\_signature_vsdir.txt"
del "%TEMP%\_signature_vsdir.txt" >nul 2>nul

if not defined VSDIR (
    echo [ERROR] Visual Studio C++ toolset not found.
    echo         vswhere ran but returned no installation path.
    exit /b 1
)

rem  Ignore the "'vswhere.exe' is not recognized" line that vcvars32.bat may
rem  print here. That comes from Visual Studio's own VsDevCmd.bat (it calls
rem  vswhere by bare name, assuming the VS Installer put it on PATH), not from
rem  this script. It is harmless -- vcvars still sets up cl correctly. Verified
rem  on this machine with MSVC 14.36.
call "!VSDIR!\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 (
    echo [ERROR] vcvars32.bat failed:
    echo         !VSDIR!\VC\Auxiliary\Build\vcvars32.bat
    exit /b 1
)

:have_toolchain

cd /d "%~dp0"
if not exist obj mkdir obj

rem  The tool = engine + resolver + the update-flow layers.
rem
rem  Split by who needs it:
rem      ENGINE      SignatureScan + SignatureDefinitionResolve
rem                  -> shared. The DLL compiles these too, if it ever scans.
rem      TOOL ONLY   SignatureGameVersion + SignatureOutputWriter
rem                  + SignatureUpdateFlow
rem                  -> the DLL needs none of these. It does not read the game
rem                     version and it does not write header files; it just
rem                     #includes offset_update.h, which is already generated.
set "ENGINE_SOURCES=SignatureScan.cpp SignatureDefinitionResolve.cpp"
set "TOOL_ONLY_SOURCES=SignatureGameVersion.cpp SignatureOutputWriter.cpp SignatureUpdateFlow.cpp"
set "TOOL_SOURCES=SignatureTool.cpp %ENGINE_SOURCES% %TOOL_ONLY_SOURCES%"

echo [1/3] Compiling signature_tool.exe ...
cl /nologo /W4 /EHsc /O2 /MT /std:c++17 /utf-8 ^
   /Fe:signature_tool.exe ^
   /Fo:obj\ ^
   /Fd:obj\ ^
   %TOOL_SOURCES% ^
   /link /SUBSYSTEM:CONSOLE

if errorlevel 1 (
    echo.
    echo [FAILED] see compiler output above
    exit /b 1
)
echo       OK

rem ---------------------------------------------------------------------------
rem  Compile the ENGINE alone, without the tool.
rem
rem  These two files are the only thing the DLL ever compiles. Building them on
rem  their own keeps the promise that "the engine compiles for the tool" implies
rem  "the engine compiles inside the DLL" -- verified on every build instead of
rem  discovered at injection time.
rem
rem  This is NOT redundant with step 1: step 1 compiles them together with
rem  SignatureTool.cpp, and a header can easily compile fine in one order and
rem  break in another. Compiling them alone is the state the DLL is in.
rem ---------------------------------------------------------------------------
rem  One file at a time: cl rejects "/Fo<prefix>" when given multiple sources,
rem  and compiling separately also makes a failure point at the right file.
echo [2/3] Compiling the engine alone (this is what the DLL compiles) ...
for %%f in (%ENGINE_SOURCES%) do (
    cl /nologo /W4 /EHsc /O2 /MT /std:c++17 /utf-8 /c ^
       /Fo:obj\dll_%%~nf.obj ^
       /Fd:obj\dll_ ^
       %%f
    if errorlevel 1 (
        echo.
        echo [FAILED] DLL-side compile failed on %%f -- see output above
        exit /b 1
    )
)
echo       OK

rem ---------------------------------------------------------------------------
rem  Run it. No arguments = update flow:
rem      read game version -> same as last time? stop. different? rescan the exe
rem      and overwrite offset_update.h
rem ---------------------------------------------------------------------------
rem  Use the full path, not the bare name: that way it runs no matter what the
rem  current directory happens to be. The tool itself uses absolute paths from
rem  SignatureConfig.h, so its own working directory does not matter.
echo [3/3] Running the update flow ...
echo.
"%~dp0signature_tool.exe"
set "TOOL_EXIT_CODE=%ERRORLEVEL%"
echo.

if "%TOOL_EXIT_CODE%"=="0" goto :result_ok
if "%TOOL_EXIT_CODE%"=="1" goto :result_partial
echo [ERROR] the update flow failed. See the output above.
goto :finish

:result_ok
echo [OK] offset_update.h is ready.
echo      Include it from the DLL. Paths live in SignatureConfig.h.
goto :finish

:result_partial
echo [WARN] offset_update.h was written, but some signatures did not resolve.
echo        The missing constants are listed at the top of offset_update.h.
echo        Fix those patterns in offsets_AI.h and run this file again.

:finish
echo.
endlocal & exit /b %TOOL_EXIT_CODE%
