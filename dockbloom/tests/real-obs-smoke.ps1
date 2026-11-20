# DockBloom for OBS Studio - Copyright (C) 2026 Emmanuel Suah, GPL-2.0-or-later.
# Install and exercise the real plugin in a disposable OBS 33.0.1 portable
# installation. A test-only companion DLL drives the actual UI, built-in and
# browser docks, and a loopback CEF page across two clean OBS launches.
param(
    [Parameter(Mandatory=$true)] [string] $PackagePath,
    [Parameter(Mandatory=$true)] [string] $ArchivePath,
    [Parameter(Mandatory=$true)] [string] $DriverPath
)
$ErrorActionPreference = 'Stop'
$work = Join-Path $env:RUNNER_TEMP 'dockbloom-real-obs'
$expectedHash = '0d9436451c2d772b33428e3fdae9776f071895c335de17283176306fb72651d3'
$obs = $null
$logDir = $null
try {
    if ((Get-FileHash $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
        throw 'OBS portable archive checksum mismatch.'
    }
    if (!(Test-Path $DriverPath)) { throw 'The test-only OBS driver DLL is missing.' }
    New-Item -ItemType Directory -Path $work -Force | Out-Null
    & tar.exe -xf $ArchivePath -C $work
    if ($LASTEXITCODE -ne 0) { throw "Cannot extract OBS archive: $LASTEXITCODE" }
    $exe = Get-ChildItem $work -Recurse -Filter obs64.exe | Select-Object -First 1
    if (!$exe) { throw 'obs64.exe not found in the OBS archive.' }
    $bin = Split-Path $exe.FullName
    $root = Split-Path (Split-Path $bin)
    New-Item -ItemType File -Path (Join-Path $root 'portable_mode.txt') -Force | Out-Null
    $config = Join-Path $root 'config\obs-studio'
    New-Item -ItemType Directory -Path $config -Force | Out-Null
    $logDir = Join-Path $config 'logs'
    $pluginRoot = Join-Path $root 'plugins'
    $plugin = Join-Path $pluginRoot 'obs-dockbloom'
    $otherPlugin = Join-Path $pluginRoot 'unrelated-plugin'
    New-Item -ItemType Directory -Path $otherPlugin -Force | Out-Null
    Set-Content -Path (Join-Path $otherPlugin 'keep.txt') -Value 'unrelated plugin' -Encoding ascii

    # Test the installer and uninstaller against the actual assembled package.
    $incomplete = Join-Path $work 'incomplete-package'
    New-Item -ItemType Directory -Path $incomplete -Force | Out-Null
    Copy-Item (Join-Path $PackagePath '*') $incomplete -Recurse
    Remove-Item (Join-Path $incomplete 'data\locale\en-US.ini')
    $refusedIncomplete = $false
    try { & (Join-Path $incomplete 'Install.ps1') -PluginRoot $pluginRoot }
    catch {
        if ($_.Exception.Message -notlike 'Incomplete package:*') { throw }
        $refusedIncomplete = $true
    }
    if (!$refusedIncomplete -or (Test-Path $plugin)) { throw 'The installer accepted an incomplete package.' }
    & (Join-Path $PackagePath 'Install.ps1') -PluginRoot $pluginRoot
    if (!(Test-Path (Join-Path $plugin 'obs-dockbloom.dll')) -or
        !(Test-Path (Join-Path $plugin 'data\locale\en-US.ini'))) { throw 'Installed plugin files are missing.' }
    Set-Content -Path (Join-Path $plugin 'keep.txt') -Value 'do not overwrite' -Encoding ascii
    $refusedOverwrite = $false
    try { & (Join-Path $PackagePath 'Install.ps1') -PluginRoot $pluginRoot }
    catch {
        if ($_.Exception.Message -notlike 'Destination already exists:*') { throw }
        $refusedOverwrite = $true
    }
    if (!$refusedOverwrite -or !(Test-Path (Join-Path $plugin 'keep.txt'))) {
        throw 'The installer failed to protect an existing plugin folder.'
    }
    & (Join-Path $PackagePath 'Uninstall.ps1') -PluginRoot $pluginRoot
    if ((Test-Path $plugin) -or !(Test-Path (Join-Path $otherPlugin 'keep.txt'))) {
        throw 'The uninstaller did not remove only the DockBloom folder.'
    }
    & (Join-Path $PackagePath 'Install.ps1') -PluginRoot $pluginRoot
    Write-Host 'Installer: incomplete package, overwrite protection, uninstall and reinstall checks passed.'

    $driverDir = Join-Path $pluginRoot 'obs-dockbloom-test-driver'
    New-Item -ItemType Directory -Path $driverDir -Force | Out-Null
    Copy-Item $DriverPath (Join-Path $driverDir 'obs-dockbloom-test-driver.dll')
    $platform = Get-ChildItem $bin -Recurse -Filter qwindows.dll | Select-Object -First 1
    if (!$platform) { throw 'The OBS distribution does not contain qwindows.dll.' }
    $env:QT_QPA_PLATFORM = 'windows'
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Split-Path $platform.FullName
    $env:QT_FORCE_STDERR_LOGGING = '1'
    $env:OBS_PLUGINS_PATH = ''

    # Choose a free loopback port; the driver will bind it in each OBS run.
    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
    $listener.Start()
    $env:DOCKBLOOM_TEST_PORT = [string]$listener.LocalEndpoint.Port
    $listener.Stop()
    $env:DOCKBLOOM_TEST_RESULT = Join-Path $work 'test-results.txt'
    $env:DOCKBLOOM_TEST_STATE = Join-Path $work 'test-state.json'
    $existing = '[{"title":"Existing browser test","url":"http://127.0.0.1:' + $env:DOCKBLOOM_TEST_PORT + '/existing","uuid":"1234567890abcdef1234567890abcdef"}]'
    "[General]`nFirstRun=true`nWarnedAboutClosingDocks=true`n[BasicWindow]`nExtraBrowserDocks=$existing`n" |
        Set-Content -Encoding UTF8 (Join-Path $config 'user.ini')

    foreach ($phase in 1, 2) {
        $env:DOCKBLOOM_TEST_PHASE = [string]$phase
        $stdout = Join-Path $work "obs-$phase-stdout.txt"
        $stderr = Join-Path $work "obs-$phase-stderr.txt"
        $obs = Start-Process -FilePath $exe.FullName -WorkingDirectory $bin -ArgumentList @(
            '--portable', '--multi', '--disable-updater', '--disable-missing-files-check'
        ) -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
        if (!$obs.WaitForExit(180000)) { throw "OBS test phase $phase timed out after 180 seconds." }
        $obs.Refresh()
        if ($obs.ExitCode -ne 0) { throw "OBS test phase $phase exited with code $($obs.ExitCode)." }
        if (!(Test-Path $env:DOCKBLOOM_TEST_RESULT)) { throw "OBS test phase $phase produced no results." }
        $results = Get-Content $env:DOCKBLOOM_TEST_RESULT -Raw
        Write-Host $results
        if ($results -notmatch "DONE phase=$phase failures=0") { throw "OBS test phase $phase did not pass." }
        if ($results -match '(?m)^FAIL ') { throw 'The real-OBS test driver reported a failed check.' }
        if (Test-Path $logDir) {
            $log = Get-ChildItem $logDir -Filter '*.txt' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if (!$log -or !(Select-String -Path $log.FullName -SimpleMatch '[obs-dockbloom] loaded' -Quiet)) {
                throw "OBS test phase $phase did not load the packaged DockBloom module."
            }
        }
        $obs = $null
    }
    Write-Host 'Real OBS 33.0.1: UI, 15 existing docks, 4-up paging, local CEF load/reopen, restart, restore and removal passed.'
    Write-Host 'This automated test does not cover all third-party plugins, authenticated sites, GPUs or live productions.'
}
catch {
    $detail = $_.Exception.Message
    if ($env:DOCKBLOOM_TEST_RESULT -and (Test-Path $env:DOCKBLOOM_TEST_RESULT)) {
        $detail += "`nDriver results:`n" + (Get-Content $env:DOCKBLOOM_TEST_RESULT -Raw)
    }
    if ($logDir -and (Test-Path $logDir)) {
        $log = Get-ChildItem $logDir -Filter '*.txt' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($log) { $detail += "`nOBS log tail:`n" + ((Get-Content $log.FullName -Tail 55) -join "`n") }
    }
    foreach ($path in (Get-ChildItem $work -Filter 'obs-*-stderr.txt' -ErrorAction SilentlyContinue)) {
        $detail += "`n$($path.Name):`n" + ((Get-Content $path.FullName -Tail 20) -join "`n")
    }
    $escaped = $detail.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
    Write-Host "::error title=Real OBS DockBloom test::$escaped"
    exit 1
}
finally {
    if ($obs) {
        $obs.Refresh()
        if (!$obs.HasExited) { Stop-Process -Id $obs.Id -Force -ErrorAction SilentlyContinue }
    }
}
