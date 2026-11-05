[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [string] $PluginRoot = (Join-Path $env:ProgramData 'obs-studio\plugins')
)
$ErrorActionPreference = 'Stop'

$name = 'obs-auto-scene'

if (Get-Process -Name 'obs64', 'obs32' -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before uninstalling the plugin.'
}

$targets = @(
    (Join-Path $PluginRoot $name),
    (Join-Path $env:APPDATA "obs-studio\plugins\$name")
)

foreach ($target in $targets) {
    if (-not (Test-Path $target)) { continue }

    if ($PSCmdlet.ShouldProcess($target, 'Remove Auto Scene Adjust')) {
        Remove-Item $target -Recurse -Force
        Write-Host "Removed $target"
    }
}

Write-Host ''
Write-Host 'Auto Scene Adjust removed. Your scenes are exactly as it left them:'
Write-Host 'every transform it applied is a normal OBS transform.'
Write-Host "Its settings are kept in $env:APPDATA\obs-studio\plugin_config\$name\"
Write-Host '- delete that folder too for a clean slate.'
