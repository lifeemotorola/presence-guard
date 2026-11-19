# MixBloom for OBS Studio - Copyright (C) 2026 Emmanuel Suah.
# Smoke-test the packaged DLL's actual load in a disposable OBS Studio 33.0.1
# portable installation. No streaming or recording is started.
param(
    [Parameter(Mandatory=$true)] [string] $PackagePath,
    [Parameter(Mandatory=$true)] [string] $ArchivePath
)
$ErrorActionPreference = 'Stop'
$work = Join-Path $env:RUNNER_TEMP 'mixbloom-real-obs'
$expectedHash = '0d9436451c2d772b33428e3fdae9776f071895c335de17283176306fb72651d3'
$obs = $null
$logDir = $null
try {
    $actual = (Get-FileHash $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expectedHash) { throw "OBS download checksum mismatch: $actual" }
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
    "[General]`nFirstRun=true`n" | Set-Content -Encoding UTF8 (Join-Path $config 'user.ini')
    $logDir = Join-Path $config 'logs'
    $pluginRoot = Join-Path $root 'plugins'
    $plugin = Join-Path $pluginRoot 'obs-mixbloom'
    $installScript = Join-Path $PackagePath 'Install.ps1'
    $uninstallScript = Join-Path $PackagePath 'Uninstall.ps1'
    $otherPlugin = Join-Path $pluginRoot 'unrelated-plugin'
    New-Item -ItemType Directory -Path $otherPlugin -Force | Out-Null
    Set-Content -Path (Join-Path $otherPlugin 'keep.txt') -Value 'unrelated plugin' -Encoding ascii
    # Exercise the actual installer in a disposable portable plugin root.
    & $installScript -PluginRoot $pluginRoot
    if (!(Test-Path (Join-Path $plugin 'obs-mixbloom.dll')) -or
        !(Test-Path (Join-Path $plugin 'data\locale\en-US.ini'))) {
        throw 'The portable installer did not place the DLL and locale data correctly.'
    }
    Set-Content -Path (Join-Path $plugin 'keep.txt') -Value 'do not overwrite' -Encoding ascii
    $refusedOverwrite = $false
    try {
        & $installScript -PluginRoot $pluginRoot
    } catch {
        if ($_.Exception.Message -notlike 'Destination already exists:*') { throw }
        $refusedOverwrite = $true
    }
    if (!$refusedOverwrite -or !(Test-Path (Join-Path $plugin 'keep.txt'))) {
        throw 'The installer did not protect an existing plugin folder.'
    }
    & $uninstallScript -PluginRoot $pluginRoot
    if ((Test-Path $plugin) -or !(Test-Path (Join-Path $otherPlugin 'keep.txt'))) {
        throw 'The uninstaller did not remove only its own plugin folder.'
    }
    & $installScript -PluginRoot $pluginRoot
    Write-Host 'Portable install, overwrite protection, uninstall and reinstall checks passed.'
    # Use the native platform plugin bundled with this OBS distribution,
    # not a platform DLL from a separately compiled Qt build.
    $platform = Get-ChildItem $bin -Recurse -Filter qwindows.dll | Select-Object -First 1
    if (!$platform) { throw 'The OBS distribution does not contain qwindows.dll.' }
    $env:QT_QPA_PLATFORM = 'windows'
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Split-Path $platform.FullName
    $env:QT_FORCE_STDERR_LOGGING = '1'
    $env:OBS_PLUGINS_PATH = ''
    $stdout = Join-Path $work 'obs-stdout.txt'
    $stderr = Join-Path $work 'obs-stderr.txt'
    $obs = Start-Process -FilePath $exe.FullName -WorkingDirectory $bin -ArgumentList @(
        '--portable', '--multi', '--disable-updater', '--disable-missing-files-check'
    ) -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    $found = $false
    for ($i = 0; $i -lt 90; $i++) {
        Start-Sleep -Seconds 1
        $obs.Refresh()
        if (Test-Path $logDir) {
            $log = Get-ChildItem $logDir -Filter '*.txt' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if ($log -and (Select-String -Path $log.FullName -SimpleMatch '[obs-mixbloom] loaded' -Quiet)) {
                $found = $true
                break
            }
        }
        if ($obs.HasExited) { break }
    }
    if (!$found) { throw 'OBS did not log the MixBloom module load within 90 seconds.' }
    # Module load occurs early in startup. Give the frontend and its initial
    # scene collection time to initialize, and catch an immediate crash.
    Start-Sleep -Seconds 5
    $obs.Refresh()
    if ($obs.HasExited) { throw "OBS exited shortly after module load (code $($obs.ExitCode))." }
    Write-Host 'Real OBS 33.0.1 loaded the packaged MixBloom DLL and remained running in portable mode.'
    # If the runner exposed a main window, also try a normal close. This is
    # optional because hosted sessions can suppress windows or show first-run
    # UI; the finally block force-cleans any process that remains.
    if ($obs.CloseMainWindow() -and $obs.WaitForExit(15000)) {
        $obs.Refresh()
        if ($obs.ExitCode -ne 0) { throw "OBS exited with code $($obs.ExitCode) after a normal close request." }
        Write-Host 'OBS exited cleanly after a normal window-close request.'
    } else {
        Write-Warning 'A normal window close was unavailable or timed out; cleanup will stop OBS.'
    }
    Write-Host 'This is a load smoke test only; it does not test real devices or the viewer output.'
}
catch {
    $detail = $_.Exception.Message
    if ($logDir -and (Test-Path $logDir)) {
        $log = Get-ChildItem $logDir -Filter '*.txt' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($log) { $detail += "`nOBS log tail:`n" + ((Get-Content $log.FullName -Tail 35) -join "`n") }
    }
    foreach ($name in @('obs-stdout.txt', 'obs-stderr.txt')) {
        $path = Join-Path $work $name
        if (Test-Path $path) { $detail += "`n${name}:`n" + ((Get-Content $path -Tail 20) -join "`n") }
    }
    $escaped = $detail.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
    Write-Host "::error title=OBS load smoke test::$escaped"
    exit 1
}
finally {
    if ($obs) {
        $obs.Refresh()
        if (!$obs.HasExited) { Stop-Process -Id $obs.Id -Force -ErrorAction SilentlyContinue }
    }
}
