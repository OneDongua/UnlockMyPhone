@echo off
setlocal

set "MODULE_DIR=%~dp0magisk"
set "DAEMON=%~dp0build\unlockd"
set "STAGING=%~dp0build\unlockd-magisk"
set "OUTPUT=%~dp0build\unlockd-magisk.zip"

if not exist "%DAEMON%" (
    echo Missing %DAEMON%
    echo Build the native daemon first with build.bat.
    exit /b 1
)

if exist "%STAGING%" rmdir /s /q "%STAGING%"
if exist "%OUTPUT%" del /q "%OUTPUT%"

mkdir "%STAGING%\system\bin"
copy /y "%MODULE_DIR%\module.prop" "%STAGING%\module.prop" >nul
copy /y "%MODULE_DIR%\service.sh" "%STAGING%\service.sh" >nul
copy /y "%DAEMON%" "%STAGING%\system\bin\unlockd" >nul

rem ZIP is created from inside the module root so Magisk sees module.prop at the root.
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%STAGING%\*' -DestinationPath '%OUTPUT%' -Force"
if errorlevel 1 exit /b 1

echo Created %OUTPUT%
endlocal
