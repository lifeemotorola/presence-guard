# DockBloom for OBS Studio -- Copyright (C) 2026 Emmanuel Suah, GPL-2.0-or-later.
[CmdletBinding(SupportsShouldProcess=$true)]
param([string] $PluginRoot = (Join-Path $env:ProgramData 'obs-studio\plugins'))
$ErrorActionPreference = 'Stop'
if (Get-Process -Name 'obs64', 'obs32' -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before uninstalling DockBloom.'
}
$target = Join-Path $PluginRoot 'obs-dockbloom'
if ((Test-Path $target) -and $PSCmdlet.ShouldProcess($target, 'Remove DockBloom')) {
    Remove-Item $target -Recurse -Force
    Write-Host "Removed $target"
}
Write-Host 'Uninstalling does not automatically restore an earlier OBS dock layout.'
Write-Host 'DockBloom settings and saved web addresses remain in the OBS plugin_config folder.'
