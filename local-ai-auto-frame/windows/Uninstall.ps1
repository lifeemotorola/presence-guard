[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [string] $PluginRoot = (Join-Path $env:ProgramData 'obs-studio\plugins')
)
$ErrorActionPreference = 'Stop'

$name = 'obs-local-ai'
$targets = @(
    (Join-Path $PluginRoot $name),
    (Join-Path $env:APPDATA "obs-studio\plugins\$name")
)

if (Get-Process -Name 'obs64', 'obs32' -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before uninstalling the plugin.'
}

$removed = 0
foreach ($target in $targets) {
    if (-not (Test-Path $target)) { continue }
    if ($PSCmdlet.ShouldProcess($target, 'Remove plugin')) {
        Remove-Item $target -Recurse -Force
        Write-Host "Removed $target"
        $removed++
    }
}

if ($removed -eq 0) {
    Write-Host "Nothing to remove: $name is not installed in $PluginRoot or %APPDATA%\obs-studio\plugins."
}
