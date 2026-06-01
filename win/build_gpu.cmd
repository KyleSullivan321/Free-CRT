@echo off
REM Build Free CRT (Smart Render + OpenCL GPU) against the AE 25.6 SDK.
REM Run from a VS x64 developer environment (call vcvars64.bat first).
setlocal
set REPO=%~dp0..
if "%AE_SDK_PATH%"=="" set AE_SDK_PATH=%REPO%\aesdk\AfterEffectsSDK_25.6_61_win\ae25.6_61.64bit.AfterEffectsSDK
set H=%AE_SDK_PATH%\Examples\Headers
set U=%AE_SDK_PATH%\Examples\Util
set RES=%AE_SDK_PATH%\Examples\Resources
set SRC=%REPO%\src
set VEND=%REPO%\vendor
set OUT=%REPO%\build\Release
set OBJ=%OUT%\obj
if not exist "%OBJ%" mkdir "%OBJ%"

echo [1/4] Generating PiPL...
cl /nologo /I "%H%" /I "%H%\SP" /EP "%REPO%\resources\FreeCRT_PiPL.r" > "%OBJ%\FreeCRT_PiPL.rr"
if errorlevel 1 goto :fail
pushd "%OBJ%"
"%RES%\PiPLtool.exe" FreeCRT_PiPL.rr FreeCRT_PiPL.rrc
popd

echo [2/4] Compiling sources...
cl /c /nologo /EHsc /O2 /std:c++17 /MD /DMSWindows /DWIN32 /D_WINDOWS /DWIN_ENV /D_CRT_SECURE_NO_WARNINGS /DCL_TARGET_OPENCL_VERSION=120 ^
   /I "%H%" /I "%H%\SP" /I "%H%\Win" /I "%U%" /I "%VEND%" /I "%SRC%" ^
   "%SRC%\FreeCRT.cpp" "%SRC%\CRT_Render.cpp" "%SRC%\CRT_Presets.cpp" "%SRC%\CRT_Strings.cpp" ^
   /Fo"%OBJ%\\"
if errorlevel 1 goto :fail

echo [3/4] Compiling resources...
rc /nologo /i "%OBJ%" /fo "%OBJ%\FreeCRT.res" "%~dp0FreeCRT.rc"
if errorlevel 1 goto :fail

echo [4/4] Linking FreeCRT.aex...
link /nologo /DLL /MACHINE:X64 /OUT:"%OUT%\FreeCRT.aex" ^
   "%OBJ%\FreeCRT.obj" "%OBJ%\CRT_Render.obj" "%OBJ%\CRT_Presets.obj" "%OBJ%\CRT_Strings.obj" ^
   "%OBJ%\FreeCRT.res" "%VEND%\OpenCL.lib" user32.lib
if errorlevel 1 goto :fail

echo BUILD OK: "%OUT%\FreeCRT.aex"
endlocal & exit /b 0
:fail
echo BUILD FAILED (errorlevel %errorlevel%)
endlocal & exit /b 1
