# package-windows.ps1 - build distributable Windows zip for glabels-neo
#
# Stages the installed build (windeployqt output is installed alongside the
# executables by the CMake install rules), adds license files and zips it.
#
# Usage:  scripts/package-windows.ps1 [-BuildDir build] [-OutZip glabels-neo-windows-x64.zip]
param(
   [string]$BuildDir = "build",
   [string]$OutZip = "glabels-neo-windows-x64.zip"
)

$ErrorActionPreference = "Stop"

$stage = Join-Path $env:RUNNER_TEMP "glabels-stage"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }

# The Visual Studio generator is multi-config: name the configuration explicitly.
cmake --install $BuildDir --config Release --prefix $stage
if ($LASTEXITCODE -ne 0) { throw "cmake --install failed" }

# gLabels' own translation catalogs must sit next to the executable.  FileUtil prefers
# <appdir>/translations (the "portable" layout, which windeployqt fills with Qt's own
# catalogs) over share/glabels-qt/translations, so without this copy the application would
# find Qt's translations directory and never load its own.
$own = Join-Path $stage "share/glabels-qt/translations"
$portable = Join-Path $stage "bin/translations"
if (Test-Path $own) {
   New-Item -ItemType Directory -Force -Path $portable | Out-Null
   Copy-Item (Join-Path $own "*.qm") $portable -Force
}

# License and notice files (GPLv3 / LGPLv3 / MIT / third-party).  Use Git for Windows' bash
# explicitly: a bare "bash" can resolve to the WSL launcher stub on GitHub-hosted runners.
$gitBash = Join-Path $env:ProgramFiles "Git\bin\bash.exe"
if (-not (Test-Path $gitBash)) { throw "Git Bash not found at $gitBash" }
$stageUnix = $stage -replace "\\", "/"
& $gitBash scripts/stage-licenses.sh $stageUnix $BuildDir
if ($LASTEXITCODE -ne 0) { throw "stage-licenses.sh failed" }

Compress-Archive -Path "$stage/*" -DestinationPath $OutZip -Force
Write-Host "Packaged: $OutZip"
