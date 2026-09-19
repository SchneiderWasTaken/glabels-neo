# package-windows.ps1 — build distributable Windows zip for glabels-neo
#
# Stages the installed build (windeployqt output is installed alongside the
# executables by the CMake install rules) and zips it.
#
# Usage:  scripts/package-windows.ps1 [-BuildDir build] [-OutZip glabels-neo-windows-x64.zip]
param(
   [string]$BuildDir = "build",
   [string]$OutZip = "glabels-neo-windows-x64.zip"
)

$ErrorActionPreference = "Stop"

$stage = Join-Path $env:RUNNER_TEMP "glabels-stage"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }

cmake --install $BuildDir --prefix $stage

Compress-Archive -Path "$stage/*" -DestinationPath $OutZip -Force
Write-Host "Packaged: $OutZip"
