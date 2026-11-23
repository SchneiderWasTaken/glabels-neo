<#
.SYNOPSIS
    Removes the gLabels Neo launcher, the installed program and the Start menu shortcut.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$Root = Join-Path $env:LOCALAPPDATA 'gLabelsNeo'

$running = Get-Process -Name 'glabels-qt', 'glabels-fill', 'glabels-batch-qt' -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -and $_.Path.StartsWith($Root, [StringComparison]::OrdinalIgnoreCase) }
if ($running) { throw 'gLabels Neo is running. Close it and run this again.' }

$lnkPath = Join-Path ([Environment]::GetFolderPath('Programs')) 'gLabels Neo.lnk'
if (Test-Path $lnkPath) { Remove-Item $lnkPath -Force; Write-Host "Removed $lnkPath" }
if (Test-Path $Root)    { Remove-Item $Root -Recurse -Force; Write-Host "Removed $Root" }
Write-Host 'gLabels Neo has been uninstalled. (Your .glabels files and the per-user settings are untouched.)'
