<#
.SYNOPSIS
    Removes the release-train trigger task and its script.
#>
$name = 'gLabels Neo release train trigger'
if (Get-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue) {
    Unregister-ScheduledTask -TaskName $name -Confirm:$false
    Write-Host "Removed '$name'."
}
Remove-Item (Join-Path $env:LOCALAPPDATA 'gLabelsNeo\Trigger-ReleaseTrain.ps1') -Force -ErrorAction SilentlyContinue
