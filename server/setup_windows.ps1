# Mobile3D Scan Server - Windows setup (PowerShell)
# Requires: Python 3.12 (64-bit) from python.org, added to PATH.
# Run once:  powershell -ExecutionPolicy Bypass -File setup_windows.ps1
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$py = $null
foreach ($cmd in @("py -3.12", "python")) {
    try {
        $v = Invoke-Expression "$cmd --version" 2>$null
        if ($v -match "3\.12") { $py = $cmd; break }
    } catch { }
}
if (-not $py) {
    Write-Host "Python 3.12 not found. Install it from https://www.python.org/downloads/ and re-run." -ForegroundColor Red
    exit 1
}
Write-Host "Using $py ($v)"

if (-not (Test-Path ".venv")) {
    Invoke-Expression "$py -m venv .venv"
}
& .\.venv\Scripts\python.exe -m pip install --upgrade pip
& .\.venv\Scripts\pip.exe install -r requirements.txt
Write-Host ""
Write-Host "Setup done. Start the server with:  .\run_windows.bat"
Write-Host "Then set the same address in the app (Scan screen), e.g. http://<this-PC-LAN-IP>:8000"
