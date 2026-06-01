@echo off
REM Double-click this file to install FreeCRT.aex into every After Effects
REM Plug-ins folder. It self-elevates (you'll get a UAC prompt -> click Yes).

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator rights...
    powershell -NoProfile -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

set "SRC=%~dp0build\Release\FreeCRT.aex"
if not exist "%SRC%" (
    echo ERROR: %SRC% not found. Build the plugin first.
    pause & exit /b 1
)

echo Installing %SRC%
for /d %%D in ("C:\Program Files\Adobe\Adobe After Effects*") do (
    rem remove any older install folder so it can't double-register
    rmdir /S /Q "%%~D\Support Files\Plug-ins\CRT Factory (open)" 2>nul
    md "%%~D\Support Files\Plug-ins\Free CRT" 2>nul
    copy /Y "%SRC%" "%%~D\Support Files\Plug-ins\Free CRT\FreeCRT.aex" >nul
    if errorlevel 1 (echo   FAILED: %%~D) else (echo   OK: %%~D)
)
echo.
echo Done. Restart After Effects, then Effect ^> Free CRT ^> Free CRT.
pause
