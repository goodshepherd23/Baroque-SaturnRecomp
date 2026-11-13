@echo off
rem Headless setup step for RecompLauncher (see recomplauncher.json).
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0kit\prepare.ps1" %*
exit /b %ERRORLEVEL%
