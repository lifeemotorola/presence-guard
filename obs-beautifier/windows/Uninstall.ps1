[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [string] $PluginRoot = (Join-Path $env:ProgramData 'obs-studio\plugins')
)
$ErrorActionPreference = 'Stop'

$name = 'obs-beautifier'

if (Get-Process -Name 'obs64', 'obs32' -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before uninstalling the plugin.'
}

$targets = @(
    (Join-Path $PluginRoot $name),
    (Join-Path $env:APPDATA "obs-studio\plugins\$name")
)

foreach ($target in $targets) {
    if (-not (Test-Path $target)) { continue }

    if ($PSCmdlet.ShouldProcess($target, 'Remove OBS Beautifier')) {
        Remove-Item $target -Recurse -Force
        Write-Host "Removed $target"
    }
}

Write-Host ''
Write-Host 'OBS Beautifier removed. OBS starts with its own theme again.'
Write-Host "Your look settings are kept in $env:APPDATA\obs-studio\plugin_config\$name\settings.json"
Write-Host '- delete that folder too for a clean slate.'
