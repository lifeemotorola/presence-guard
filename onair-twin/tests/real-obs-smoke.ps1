# OnAirTwin for OBS Studio - Copyright (C) 2026 Emmanuel Suah.
# Smoke-test the packaged DLL's actual load in a disposable OBS Studio 33.0.1
# portable installation. No streaming or recording is started.
param(
    [Parameter(Mandatory=$true)] [string] $PackagePath,
    [Parameter(Mandatory=$true)] [string] $ArchivePath
)
$ErrorActionPreference = 'Stop'
$work = Join-Path $env:RUNNER_TEMP 'onair-twin-real-obs'
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
    $plugin = Join-Path $root 'plugins\obs-onair-twin'
    New-Item -ItemType Directory -Path $plugin -Force | Out-Null
    Copy-Item (Join-Path $PackagePath 'bin\64bit\obs-onair-twin.dll') $plugin
    Copy-Item (Join-Path $PackagePath 'data') $plugin -Recurse
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
            if ($log -and (Select-String -Path $log.FullName -SimpleMatch '[obs-onair-twin] loaded' -Quiet)) {
                $found = $true
                break
            }
        }
        if ($obs.HasExited) { break }
    }
    if (!$found) { throw 'OBS did not log the OnAirTwin module load within 90 seconds.' }
    Write-Host 'Real OBS 33.0.1 loaded the packaged OnAirTwin DLL in portable mode.'
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
