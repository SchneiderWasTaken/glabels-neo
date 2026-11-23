<#
.SYNOPSIS
    Deploys the report relay as a DigitalOcean Function and prints its URL.

.DESCRIPTION
    Needs doctl, signed in (doctl auth init), with the serverless plugin installed and connected to a Functions
    namespace (doctl serverless install; doctl serverless namespaces create --label glabels-neo-relay --region sfo3;
    doctl serverless connect).  Secrets never touch the repository:
      - the GitHub token comes from -TokenFile (or the RELAY_GITHUB_TOKEN environment variable); it must be a
        fine-grained token with "Issues: Read and write" on this one repository and nothing else;
      - the challenge-signing secret is generated once and kept in %LOCALAPPDATA%\gLabelsNeo.

.PARAMETER TokenFile
    File containing the GitHub token.
#>
[CmdletBinding()]
param([string]$TokenFile)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

$doctl = (Get-Command doctl -ErrorAction SilentlyContinue).Source
if (-not $doctl) {
    $doctl = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter doctl.exe -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $doctl) { throw 'doctl was not found.' }

$token = $env:RELAY_GITHUB_TOKEN
if ($TokenFile) { $token = (Get-Content -Raw -Path $TokenFile).Trim() }
if (-not $token) { throw 'Give the GitHub token with -TokenFile or RELAY_GITHUB_TOKEN.' }

$state = Join-Path $env:LOCALAPPDATA 'gLabelsNeo'
New-Item -ItemType Directory -Force -Path $state | Out-Null
$secretFile = Join-Path $state 'relay-challenge-secret.txt'
if (-not (Test-Path $secretFile)) {
    $bytes = New-Object byte[] 32
    [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
    ([BitConverter]::ToString($bytes) -replace '-', '').ToLower() | Set-Content -Path $secretFile -NoNewline
}
$challenge = (Get-Content -Raw -Path $secretFile).Trim()

& node (Join-Path $here 'stage.mjs')
if ($LASTEXITCODE -ne 0) { throw 'staging failed' }

$envFile = Join-Path $env:TEMP ("glabels-relay-" + [guid]::NewGuid().ToString('N') + '.env')
try {
    Set-Content -Path $envFile -Value @("GITHUB_TOKEN=$token", "CHALLENGE_SECRET=$challenge") -Encoding ASCII
    & $doctl serverless deploy $here --env $envFile
    if ($LASTEXITCODE -ne 0) { throw 'doctl serverless deploy failed' }
} finally {
    Remove-Item $envFile -Force -ErrorAction SilentlyContinue
}

$url = (& $doctl serverless functions get relay/report --url).Trim()
Write-Host ''
Write-Host "Relay URL (set it as the repository variable REPORT_ENDPOINT):"
Write-Host $url
