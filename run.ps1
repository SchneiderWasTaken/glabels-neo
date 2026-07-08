$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $here 'build\glabels\glabels-qt.exe'

if (-not (Test-Path -LiteralPath $exe)) {
    Write-Host "Executable not found at: $exe" -ForegroundColor Red
    Write-Host "Build the project first with: cmake --build `"$here\build`" -j"
    exit 1
}

$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:PATH"

Write-Host "Launching glabels-qt..." -ForegroundColor Green
Start-Process -FilePath $exe
