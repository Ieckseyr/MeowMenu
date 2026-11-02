@echo off
rem run.bat - MeowMenu <-> HologramLib ABI conformance check
rem
rem Two layers of verification:
rem   1. Compile time: struct sizes/offsets + implementing the real interfaces with the
rem      mirrored signatures (any mismatch fails the compile).
rem   2. Text diff: vtable slot order / struct field order / version and export symbol
rem      (check_abi.js).
rem
rem Usage:
rem   run.bat                               rem looks for ..\..\..\HologramLib\include
rem   run.bat <HologramLib include folder>
rem
rem Requires: Visual Studio build tools (cl.exe) + node
setlocal enabledelayedexpansion
set HERE=%~dp0
set REAL_INC=%~1
if "%REAL_INC%"=="" set REAL_INC=%HERE%..\..\..\HologramLib\include

if not exist "%REAL_INC%\hologramlib\HologramLib.h" (
    echo [ERROR] real header not found: %REAL_INC%\hologramlib\HologramLib.h
    echo         pass it explicitly: run.bat ^<HologramLib include folder^>
    exit /b 2
)

rem -- MSVC environment --
where cl.exe >nul 2>nul
if errorlevel 1 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -property installationPath`) do set "VSPATH=%%i"
    )
    if not defined VSPATH if exist "D:\VS\VS\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=D:\VS\VS"
    if defined VSPATH (
        call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul
    ) else (
        echo [ERROR] MSVC environment not found, install Visual Studio build tools
        exit /b 2
    )
)

set OUTDIR=%TEMP%\meowmenu_abi_test
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo === 1/2 compile-time ABI check ===
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /I "%HERE%..\..\src" /I "%REAL_INC%" ^
   "%HERE%abi_conformance.cpp" /Fo"%OUTDIR%\\" /Fe"%OUTDIR%\abi_conformance.exe" > "%OUTDIR%\build.log" 2>&1
if errorlevel 1 (
    echo [FAIL] compile-time ABI check failed, see %OUTDIR%\build.log
    findstr /C:"error" "%OUTDIR%\build.log"
    exit /b 1
)
echo [PASS] struct sizes/offsets and all override signatures match the real interfaces
"%OUTDIR%\abi_conformance.exe"
if errorlevel 1 (
    echo [FAIL] conformance binary returned non-zero
    exit /b 1
)

echo.
echo === 2/2 declaration order and version diff ===
node "%HERE%check_abi.js" "%REAL_INC%\hologramlib\HologramLib.h" "%HERE%..\..\src\HologramLibAbi.h"
if errorlevel 1 exit /b 1

rem -- optional: verify the runtime symbol against the deployed DLL --
rem The header can be newer than the built DLL, so check the actual export table too.
set LIB_DLL=%HERE%..\..\..\HologramLib\HologramLib.dll
if not exist "%LIB_DLL%" set LIB_DLL=%HERE%..\..\..\HologramLib\bin\HologramLib\HologramLib.dll
if exist "%LIB_DLL%" (
    echo.
    echo === 3/3 exported symbol in deployed DLL ===
    dumpbin /exports "%LIB_DLL%" > "%OUTDIR%\exports.txt" 2>&1
    findstr /C:"?getInstance@IHologramLib@hologramlib@@SAAEAV12@XZ" "%OUTDIR%\exports.txt" >nul
    if errorlevel 1 (
        echo [FAIL] deployed DLL does not export the getInstance symbol
        echo        MeowMenu would silently disable the NPC menu feature at runtime
        exit /b 1
    )
    echo [PASS] deployed DLL exports the expected getInstance symbol
)

echo.
echo [ALL PASS] ABI mirror matches HologramLib
exit /b 0
