<#
.SYNOPSIS
    Installs the gLabels Neo auto-updating launcher and a Start menu shortcut for the current user.

.DESCRIPTION
    Copies the launcher to %LOCALAPPDATA%\gLabelsNeo, downloads the current gLabels Neo build, and
    creates "gLabels Neo" in the Start menu.  No administrator rights are needed.  Run it again at any
    time to repair or to switch channel.

.PARAMETER Channel
    'edge'   (default) the newest build of the main branch
    'stable' the latest stable release

.PARAMETER NoShortcut
    Install and download, but do not create the Start menu shortcut.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\Install-GlabelsNeoLauncher.ps1
#>
[CmdletBinding()]
param(
    [ValidateSet('edge', 'stable')]
    [string]$Channel,
    [switch]$NoShortcut
)

$ErrorActionPreference = 'Stop'

$Root = Join-Path $env:LOCALAPPDATA 'gLabelsNeo'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
New-Item -ItemType Directory -Force -Path $Root | Out-Null

foreach ($f in 'glabels-neo-launcher.ps1', 'glabels-neo-launcher.vbs') {
    Copy-Item -Path (Join-Path $Here $f) -Destination $Root -Force
}

# Keep an existing config unless a channel was asked for explicitly.
$configPath = Join-Path $Root 'config.json'
if ($Channel -or -not (Test-Path $configPath)) {
    $chan = if ($Channel) { $Channel } else { 'edge' }
    [ordered]@{ channel = $chan; verifyAttestation = $true } | ConvertTo-Json | Set-Content -Path $configPath -Encoding UTF8
}

Write-Host 'Downloading gLabels Neo...'
$launcher = Join-Path $Root 'glabels-neo-launcher.ps1'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $launcher -NoLaunch
if ($LASTEXITCODE -ne 0) { throw "the first download failed; see $(Join-Path $Root 'launcher.log')" }

$exe = Join-Path $Root 'app\bin\glabels-qt.exe'
if (-not (Test-Path $exe)) { throw "gLabels Neo was not installed; see $(Join-Path $Root 'launcher.log')" }

if (-not $NoShortcut) {
    $lnkPath = Join-Path ([Environment]::GetFolderPath('Programs')) 'gLabels Neo.lnk'
    $shell = New-Object -ComObject WScript.Shell
    $lnk = $shell.CreateShortcut($lnkPath)
    $lnk.TargetPath       = Join-Path $env:SystemRoot 'System32\wscript.exe'
    $lnk.Arguments        = '//nologo "{0}"' -f (Join-Path $Root 'glabels-neo-launcher.vbs')
    $lnk.WorkingDirectory = $Root
    $lnk.IconLocation     = "$exe,0"
    $lnk.Description      = 'gLabels Neo - updates itself from GitHub, then starts'
    $lnk.Save()
    Write-Host "Start menu shortcut: $lnkPath"
}

Write-Host ''
Write-Host 'Done. Open the Start menu and type "gLabels Neo".'
Write-Host "Log:    $(Join-Path $Root 'launcher.log')"
Write-Host "Config: $configPath"
