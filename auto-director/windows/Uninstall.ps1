[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [string] $PluginRoot = (Join-Path $env:ProgramData 'obs-studio\plugins')
)
$ErrorActionPreference = 'Stop'

$name = 'obs-auto-director'

if (Get-Process -Name 'obs64', 'obs32' -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before uninstalling the plugin.'
}

$targets = @(
    (Join-Path $PluginRoot $name),
    (Join-Path $env:APPDATA "obs-studio\plugins\$name")
)

foreach ($target in $targets) {
    if (-not (Test-Path $target)) { continue }

    if ($PSCmdlet.ShouldProcess($target, 'Remove OBS Auto Director')) {
        Remove-Item $target -Recurse -Force
        Write-Host "Removed $target"
    }
}

Write-Host ''
Write-Host 'Auto Director removed. Your takes and settings are kept in'
Write-Host "$env:APPDATA\obs-studio\plugin_config\$name\settings.json - delete that folder too for a clean slate."
