<#
.SYNOPSIS
    Installs a per-user scheduled task that triggers the release train inside the release windows.
#>
[CmdletBinding()]
param([string]$Repo = 'SchneiderWasTaken/glabels-neo')

$ErrorActionPreference = 'Stop'
$name = 'gLabels Neo release train trigger'
$root = Join-Path $env:LOCALAPPDATA 'gLabelsNeo'
New-Item -ItemType Directory -Force -Path $root | Out-Null

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$script = Join-Path $root 'Trigger-ReleaseTrain.ps1'
Copy-Item -Path (Join-Path $here 'Trigger-ReleaseTrain.ps1') -Destination $script -Force

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw 'The GitHub CLI (gh) is required.' }
& gh auth status *> $null
if ($LASTEXITCODE -ne 0) { throw 'gh is not signed in (run: gh auth login).' }

$arg = '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "{0}" -Repo {1}' -f $script, $Repo
$action  = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument $arg
$trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).Date -RepetitionInterval (New-TimeSpan -Minutes 10)
$settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit (New-TimeSpan -Minutes 5) -MultipleInstances IgnoreNew
$principal = New-ScheduledTaskPrincipal -UserId ([Security.Principal.WindowsIdentity]::GetCurrent().Name) -LogonType Interactive

Register-ScheduledTask -TaskName $name -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Force | Out-Null
Write-Host "Installed '$name' (every 10 minutes; acts only inside the 10:00 and 14:00 Central windows)."
Write-Host "Log: $(Join-Path $root 'release-train-trigger.log')"
