[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [string] $PluginRoot = (Join-Path $env:ProgramData 'obs-studio\plugins')
)
$ErrorActionPreference = 'Stop'

$name = 'obs-hype-clip'

if (Get-Process -Name 'obs64', 'obs32' -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before uninstalling the plugin.'
}

$targets = @(
    (Join-Path $PluginRoot $name),
    (Join-Path $env:APPDATA "obs-studio\plugins\$name")
)

foreach ($target in $targets) {
    if (-not (Test-Path $target)) { continue }

    if ($PSCmdlet.ShouldProcess($target, 'Remove Hype Clip')) {
        Remove-Item $target -Recurse -Force
        Write-Host "Removed $target"
    }
}

Write-Host ''
Write-Host 'Hype Clip removed. Automatic clipping stops with it; the clips it saved stay where they are.'
Write-Host "Its settings and clip list are kept in $env:APPDATA\obs-studio\plugin_config\$name\"
Write-Host '- delete that folder too for a clean slate.'
