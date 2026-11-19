# MixBloom for OBS Studio -- Copyright (C) 2026 Emmanuel Suah, GPL-2.0-or-later.
[CmdletBinding(SupportsShouldProcess=$true)]
param([string] $PluginRoot = (Join-Path $env:ProgramData 'obs-studio\plugins'))
$ErrorActionPreference = 'Stop'
if (Get-Process -Name 'obs64', 'obs32' -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before uninstalling MixBloom.'
}
$target = Join-Path $PluginRoot 'obs-mixbloom'
if ((Test-Path $target) -and $PSCmdlet.ShouldProcess($target, 'Remove MixBloom')) {
    Remove-Item $target -Recurse -Force
    Write-Host "Removed $target"
}
Write-Host 'Uninstalling does not undo source levels, mute, monitoring or track routing changed in OBS.'
