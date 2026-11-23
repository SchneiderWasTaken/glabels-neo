<#
.SYNOPSIS
    Starts the gLabels Neo release train when the Central Time clock is inside a release window.
.DESCRIPTION
    Windows: 10:00-11:30 and 14:00-15:30 America/Chicago (same as scripts in .github/agents/policy.mjs).
    The workflow decides whether anything is released; this script only supplies a reliable clock.
#>
[CmdletBinding()]
param([string]$Repo = 'SchneiderWasTaken/glabels-neo')

$ErrorActionPreference = 'Stop'
$logDir = Join-Path $env:LOCALAPPDATA 'gLabelsNeo'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$log = Join-Path $logDir 'release-train-trigger.log'

function Write-Log([string]$m) {
    if ((Test-Path $log) -and (Get-Item $log).Length -gt 100KB) { Remove-Item $log -Force }
    Add-Content -Path $log -Value ("{0:yyyy-MM-dd HH:mm:ss}  {1}" -f (Get-Date), $m)
}

$tz      = [TimeZoneInfo]::FindSystemTimeZoneById('Central Standard Time')   # observes daylight saving
$central = [TimeZoneInfo]::ConvertTimeFromUtc([DateTime]::UtcNow, $tz)
$minutes = $central.Hour * 60 + $central.Minute
$inWindow = (($minutes -ge 600) -and ($minutes -lt 690)) -or (($minutes -ge 840) -and ($minutes -lt 930))
if (-not $inWindow) { exit 0 }

try {
    $out = & gh workflow run release-train.yml -R $Repo -f dry_run=false -f force=false 2>&1
    Write-Log ("dispatched at Central {0:HH:mm}: {1}" -f $central, ($out -join ' '))
} catch {
    Write-Log ("failed: {0}" -f $_.Exception.Message)
}
