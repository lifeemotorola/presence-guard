# OnAirTwin for OBS Studio -- Copyright (C) 2026 Emmanuel Suah, GPL-2.0-or-later.
[CmdletBinding(SupportsShouldProcess=$true)]
param([string] $PluginRoot = (Join-Path $env:ProgramData 'obs-studio\plugins'))
$ErrorActionPreference = 'Stop'
if (Get-Process -Name 'obs64', 'obs32' -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before uninstalling OnAirTwin.'
}
$target = Join-Path $PluginRoot 'obs-onair-twin'
if ((Test-Path $target) -and $PSCmdlet.ShouldProcess($target, 'Remove OnAirTwin')) {
    Remove-Item $target -Recurse -Force
    Write-Host "Removed $target"
}
Write-Host 'Your saved baseline remains in the OBS plugin_config\obs-onair-twin folder.'
