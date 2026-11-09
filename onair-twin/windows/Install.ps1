# OnAirTwin for OBS Studio -- Copyright (C) 2026 Emmanuel Suah, GPL-2.0-or-later.
[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [switch] $LegacyLayout,
    [string] $PluginRoot = (Join-Path $env:ProgramData 'obs-studio\plugins')
)
$ErrorActionPreference = 'Stop'
$name = 'obs-onair-twin'
$src = $PSScriptRoot
$dst = Join-Path $PluginRoot $name

if (Get-Process -Name 'obs64', 'obs32' -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before installing OnAirTwin.'
}
foreach ($f in @('bin\64bit\obs-onair-twin.dll', 'data\locale\en-US.ini')) {
    if (-not (Test-Path (Join-Path $src $f))) {
        throw "Incomplete package: $f is missing. Download the Actions artifact, not the source ZIP."
    }
}
if (Test-Path $dst) {
    throw "Destination already exists: $dst. Back it up and remove the old copy before installing."
}
if ($PSCmdlet.ShouldProcess($dst, 'Install OnAirTwin for OBS')) {
    $binaryDir = if ($LegacyLayout) { Join-Path $dst 'bin\64bit' } else { $dst }
    try {
        New-Item -ItemType Directory -Path $binaryDir -Force | Out-Null
        Copy-Item (Join-Path $src 'bin\64bit\obs-onair-twin.dll') $binaryDir
        Copy-Item (Join-Path $src 'data') $dst -Recurse
    }
    catch {
        Remove-Item $dst -Recurse -Force -ErrorAction SilentlyContinue
        throw "Could not install to $dst. Run PowerShell as administrator. $($_.Exception.Message)"
    }
}
Write-Host "OnAirTwin installed to $dst"
Write-Host 'Restart OBS Studio 33, then open Docks -> OnAirTwin.'
Write-Host 'OnAirTwin is a read-only simulator; test actual cameras, microphones and output separately.'
