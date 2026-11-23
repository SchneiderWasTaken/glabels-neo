<#
.SYNOPSIS
    Starts gLabels Neo, first updating it from GitHub when a newer build is available.

.DESCRIPTION
    Looks up the current gLabels Neo release on GitHub, compares its Windows package with the one
    installed under %LOCALAPPDATA%\gLabelsNeo, and when they differ downloads the new package,
    verifies it (SHA-256 from the release, plus the build-provenance attestation when the GitHub
    CLI is installed and signed in), swaps it in, and then starts the program.

    The update check never blocks starting the program: if GitHub cannot be reached, or an update
    fails or cannot be applied (for example because gLabels Neo is already running), the installed
    version is started and the reason is written to the log.

.PARAMETER Channel
    'edge'   the newest build of the main branch (default)
    'stable' the latest stable release
    Overrides the channel in config.json.

.PARAMETER CheckOnly
    Report whether an update is available and change nothing.

.PARAMETER ForceUpdate
    Reinstall even when the installed build is already current.

.PARAMETER NoLaunch
    Update if needed, but do not start the program.

.PARAMETER AppArgs
    Everything else is passed to gLabels Neo (for example a .glabels file to open).
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('edge', 'stable')]
    [string]$Channel,
    [switch]$CheckOnly,
    [switch]$ForceUpdate,
    [switch]$NoLaunch,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
try { [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12 } catch { }

$Repo       = 'SchneiderWasTaken/glabels-neo'
$AssetName  = 'glabels-neo-windows-x64.zip'
$Root       = Join-Path $env:LOCALAPPDATA 'gLabelsNeo'
$AppDir     = Join-Path $Root 'app'
$ExePath    = Join-Path $AppDir 'bin\glabels-qt.exe'
$InfoPath   = Join-Path $Root 'installed.json'
$ConfigPath = Join-Path $Root 'config.json'
$LogPath    = Join-Path $Root 'launcher.log'

New-Item -ItemType Directory -Force -Path $Root | Out-Null

function Write-Log([string]$Message) {
    $line = '{0:yyyy-MM-dd HH:mm:ss}  {1}' -f (Get-Date), $Message
    try {
        # Keep the log small: start over when it grows past 200 KB.
        if ((Test-Path $LogPath) -and ((Get-Item $LogPath).Length -gt 200KB)) { Remove-Item $LogPath -Force }
        Add-Content -Path $LogPath -Value $line -Encoding UTF8
    } catch { }
    Write-Verbose $Message
}

function Get-Config {
    $cfg = [ordered]@{ channel = 'edge'; verifyAttestation = $true }
    if (Test-Path $ConfigPath) {
        try {
            $saved = Get-Content $ConfigPath -Raw | ConvertFrom-Json
            if ($saved.channel) { $cfg.channel = [string]$saved.channel }
            if ($null -ne $saved.verifyAttestation) { $cfg.verifyAttestation = [bool]$saved.verifyAttestation }
        } catch { Write-Log "ignoring unreadable config.json: $_" }
    }
    return $cfg
}

function Get-Installed {
    if (-not (Test-Path $InfoPath)) { return $null }
    try { return (Get-Content $InfoPath -Raw | ConvertFrom-Json) } catch { return $null }
}

function Get-Remote([string]$Chan) {
    $url = if ($Chan -eq 'edge') { "https://api.github.com/repos/$Repo/releases/tags/edge" }
           else                  { "https://api.github.com/repos/$Repo/releases/latest" }
    $headers = @{ 'User-Agent' = 'glabels-neo-launcher'; 'Accept' = 'application/vnd.github+json' }
    $rel = Invoke-RestMethod -Uri $url -Headers $headers -TimeoutSec 8

    $zip  = $rel.assets | Where-Object { $_.name -eq $AssetName } | Select-Object -First 1
    if (-not $zip) { throw "release $($rel.tag_name) has no $AssetName" }
    $sums = $rel.assets | Where-Object { $_.name -eq 'SHA256SUMS' } | Select-Object -First 1

    $hash = $null
    if ($sums) {
        $resp = Invoke-WebRequest -Uri $sums.browser_download_url -Headers $headers -UseBasicParsing -TimeoutSec 8
        $text = if ($resp.Content -is [byte[]]) { [Text.Encoding]::UTF8.GetString($resp.Content) } else { [string]$resp.Content }
        foreach ($line in ($text -split "`n")) {
            if ($line -match '^([0-9a-fA-F]{64})\s+\*?(\S+)\s*$' -and $Matches[2] -eq $AssetName) { $hash = $Matches[1].ToLower() }
        }
    }
    if (-not $hash -and $zip.digest -match '^sha256:([0-9a-fA-F]{64})$') { $hash = $Matches[1].ToLower() }
    if (-not $hash) { throw "no SHA-256 available for $($rel.tag_name); refusing to install an unverifiable package" }

    [pscustomobject]@{
        channel = $Chan
        tag     = $rel.tag_name
        name    = $rel.name
        url     = $zip.browser_download_url
        size    = [long]$zip.size
        sha256  = $hash
    }
}

# ---- small progress window, shown only while an update is being installed ---------------------
$script:Ui = $null
function Show-Progress([string]$Text) {
    try {
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing
        $form = New-Object System.Windows.Forms.Form
        $form.Text = 'gLabels Neo'
        $form.StartPosition = 'CenterScreen'
        $form.FormBorderStyle = 'FixedDialog'
        $form.ControlBox = $false
        $form.TopMost = $true
        $form.ClientSize = New-Object System.Drawing.Size(380, 86)
        $label = New-Object System.Windows.Forms.Label
        $label.Location = New-Object System.Drawing.Point(16, 14)
        $label.Size = New-Object System.Drawing.Size(348, 22)
        $label.Text = $Text
        $bar = New-Object System.Windows.Forms.ProgressBar
        $bar.Location = New-Object System.Drawing.Point(16, 44)
        $bar.Size = New-Object System.Drawing.Size(348, 20)
        $bar.Style = 'Marquee'
        $form.Controls.Add($label)
        $form.Controls.Add($bar)
        $form.Show()
        [System.Windows.Forms.Application]::DoEvents()
        $script:Ui = [pscustomobject]@{ Form = $form; Label = $label; Bar = $bar }
    } catch { $script:Ui = $null }
}
function Update-Progress([string]$Text, [int]$Percent = -1) {
    if (-not $script:Ui) { return }
    try {
        $script:Ui.Label.Text = $Text
        if ($Percent -ge 0) { $script:Ui.Bar.Style = 'Continuous'; $script:Ui.Bar.Value = [Math]::Min(100, $Percent) }
        [System.Windows.Forms.Application]::DoEvents()
    } catch { }
}
function Step-Ui { if ($script:Ui) { try { [System.Windows.Forms.Application]::DoEvents() } catch { } } }
function Close-Progress {
    if (-not $script:Ui) { return }
    try { $script:Ui.Form.Close(); $script:Ui.Form.Dispose() } catch { }
    $script:Ui = $null
}

function Save-Download($Remote, [string]$Dest) {
    # BITS gives resumable, polite downloads and lets the progress window stay alive; fall back to a
    # plain download if BITS is unavailable.
    try {
        $job = Start-BitsTransfer -Source $Remote.url -Destination $Dest -Asynchronous -DisplayName 'gLabels Neo update'
        $clock = [Diagnostics.Stopwatch]::StartNew()
        while ($job.JobState -in 'Queued', 'Connecting', 'Transferring', 'TransientError') {
            # BytesTotal is a huge sentinel until BITS has learned the real size; only trust it below 10 GB.
            $total = [double]$job.BytesTotal
            if ($total -gt 0 -and $total -lt 10GB) {
                $done = [double]$job.BytesTransferred
                $pct  = [int][Math]::Min(100, [Math]::Floor(100 * $done / $total))
                Update-Progress ('Downloading gLabels Neo ({0:N0} / {1:N0} MB)' -f ($done / 1MB), ($total / 1MB)) $pct
            }
            if ($clock.Elapsed.TotalMinutes -gt 10) { Remove-BitsTransfer $job; throw "BITS timed out" }
            Start-Sleep -Milliseconds 150
            Step-Ui
        }
        if ($job.JobState -ne 'Transferred') { $state = $job.JobState; Remove-BitsTransfer $job; throw "BITS ended in state $state" }
        Complete-BitsTransfer $job
    } catch {
        Write-Log "BITS download failed ($_); using a plain download"
        Update-Progress 'Downloading gLabels Neo...'
        Invoke-WebRequest -Uri $Remote.url -OutFile $Dest -UseBasicParsing -Headers @{ 'User-Agent' = 'glabels-neo-launcher' }
    }
}

function Test-Attestation([string]$Zip, $Cfg) {
    if (-not $Cfg.verifyAttestation) { return }
    $gh = Get-Command gh -ErrorAction SilentlyContinue
    if (-not $gh) { Write-Log 'gh not installed: skipping provenance attestation check'; return }
    & gh auth status *> $null
    if ($LASTEXITCODE -ne 0) { Write-Log 'gh not signed in: skipping provenance attestation check'; return }
    & gh attestation verify $Zip --repo $Repo *> $null
    if ($LASTEXITCODE -ne 0) { throw 'build provenance attestation did not verify' }
    Write-Log 'provenance attestation verified'
}

# True when any gLabels Neo program from our install folder (including a replaced copy) is running.
# Replacing files under a running program can make it load mismatched plugins, so updates wait.
function Test-AppRunning {
    $procs = Get-Process -Name 'glabels-qt', 'glabels-fill', 'glabels-batch-qt' -ErrorAction SilentlyContinue
    foreach ($p in $procs) {
        try { if ($p.Path -and $p.Path.StartsWith($Root, [StringComparison]::OrdinalIgnoreCase)) { return $true } } catch { }
    }
    return $false
}

function Install-Update($Remote, $Cfg) {
    $work = Join-Path $Root 'staging'
    if (Test-Path $work) { Remove-Item $work -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $work | Out-Null
    try {
        $zip = Join-Path $work $AssetName
        Update-Progress 'Downloading gLabels Neo...'
        Save-Download $Remote $zip

        Update-Progress 'Verifying download...'
        $actual = (Get-FileHash -Path $zip -Algorithm SHA256).Hash.ToLower()
        if ($actual -ne $Remote.sha256) { throw "checksum mismatch (expected $($Remote.sha256), got $actual)" }
        Test-Attestation $zip $Cfg

        Update-Progress 'Unpacking...'
        $newDir = Join-Path $work 'app'
        Expand-Archive -Path $zip -DestinationPath $newDir -Force
        if (-not (Test-Path (Join-Path $newDir 'bin\glabels-qt.exe'))) { throw 'package does not contain bin\glabels-qt.exe' }

        Update-Progress 'Installing...'
        # Old copies are removed on a best-effort basis; a unique name per update means a copy that
        # cannot be deleted yet (still in use) never blocks the next update.
        Get-ChildItem -Path $Root -Directory -Filter 'app.old*' -ErrorAction SilentlyContinue |
            ForEach-Object { Remove-Item $_.FullName -Recurse -Force -ErrorAction SilentlyContinue }
        $oldDir = Join-Path $Root ('app.old.' + [guid]::NewGuid().ToString('N').Substring(0, 8))
        if (Test-Path $AppDir) { Move-Item -Path $AppDir -Destination $oldDir -ErrorAction Stop }
        try {
            Move-Item -Path $newDir -Destination $AppDir -ErrorAction Stop
        } catch {
            if (Test-Path $oldDir) { Move-Item -Path $oldDir -Destination $AppDir -ErrorAction SilentlyContinue }
            throw
        }
        if (Test-Path $oldDir) { Remove-Item $oldDir -Recurse -Force -ErrorAction SilentlyContinue }

        [pscustomobject]@{
            channel     = $Remote.channel
            tag         = $Remote.tag
            name        = $Remote.name
            sha256      = $Remote.sha256
            installedAt = (Get-Date).ToString('s')
        } | ConvertTo-Json | Set-Content -Path $InfoPath -Encoding UTF8
        Write-Log "installed $($Remote.name) [$($Remote.channel)] sha256 $($Remote.sha256.Substring(0, 12))..."
    } finally {
        Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# ---- main ---------------------------------------------------------------------------------------
$cfg  = Get-Config
$chan = if ($Channel) { $Channel } else { $cfg.channel }
$installed = Get-Installed

$remote = $null
try { $remote = Get-Remote $chan } catch { Write-Log "could not check for updates: $_" }

$installedOk = (Test-Path $ExePath) -and $installed
$needUpdate = $remote -and ($ForceUpdate -or -not $installedOk -or $installed.channel -ne $chan -or $installed.sha256 -ne $remote.sha256)

if ($needUpdate -and (Test-AppRunning)) {
    Write-Log 'update deferred: gLabels Neo is running (it will update the next time it is started while closed)'
    $needUpdate = $false
}

if ($CheckOnly) {
    [pscustomobject]@{
        Channel         = $chan
        Installed       = if ($installedOk) { $installed.name } else { '(none)' }
        Available       = if ($remote) { $remote.name } else { '(could not check)' }
        UpdateAvailable = [bool]$needUpdate
    }
    return
}

if ($needUpdate) {
    $mutex = New-Object System.Threading.Mutex($false, 'Local\gLabelsNeoLauncher')
    $haveLock = $false
    try {
        try { $haveLock = $mutex.WaitOne(120000) } catch [System.Threading.AbandonedMutexException] { $haveLock = $true }
        # Another launcher may have finished the same update while we waited.
        $installed = Get-Installed
        if ($haveLock -and ($ForceUpdate -or -not (Test-Path $ExePath) -or -not $installed -or $installed.sha256 -ne $remote.sha256)) {
            Show-Progress ('Updating gLabels Neo to {0}...' -f $remote.name)
            try { Install-Update $remote $cfg }
            catch {
                Write-Log "update failed: $_"
                if (-not (Test-Path $ExePath)) {
                    Close-Progress
                    try {
                        Add-Type -AssemblyName System.Windows.Forms
                        [void][System.Windows.Forms.MessageBox]::Show("gLabels Neo could not be installed:`n`n$($_.Exception.Message)`n`nDetails: $LogPath", 'gLabels Neo', 'OK', 'Error')
                    } catch { }
                    exit 1
                }
            } finally { Close-Progress }
        }
    } finally {
        if ($haveLock) { $mutex.ReleaseMutex() }
        $mutex.Dispose()
    }
}

if ($NoLaunch) { return }

if (-not (Test-Path $ExePath)) {
    Write-Log 'nothing installed and no update available; cannot start'
    try {
        Add-Type -AssemblyName System.Windows.Forms
        [void][System.Windows.Forms.MessageBox]::Show("gLabels Neo is not installed and could not be downloaded.`n`nDetails: $LogPath", 'gLabels Neo', 'OK', 'Error')
    } catch { }
    exit 1
}

$startArgs = @{ FilePath = $ExePath; WorkingDirectory = (Split-Path $ExePath) }
if ($AppArgs -and $AppArgs.Count -gt 0) { $startArgs.ArgumentList = $AppArgs }
Start-Process @startArgs
