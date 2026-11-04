@echo off
REM Mobile3D Scan Server - Windows launcher
setlocal
cd /d %~dp0
if not exist .venv (
  echo Virtual environment missing. Run setup_windows.ps1 first.
  pause
  exit /b 1
)
if "%SCAN_HOST%"=="" set SCAN_HOST=0.0.0.0
if "%SCAN_PORT%"=="" set SCAN_PORT=8000
echo Starting scan server on %SCAN_HOST%:%SCAN_PORT% ...
.venv\Scripts\uvicorn.exe app.main:app --host %SCAN_HOST% --port %SCAN_PORT%
pause
