[CmdletBinding(SupportsShouldProcess=$true)]
param()
$ErrorActionPreference = 'Stop'
$dst = Join-Path $env:APPDATA 'obs-studio\plugins\obs-local-ai'
if (Get-Process -Name 'obs64','obs32' -ErrorAction SilentlyContinue) { throw 'Close OBS Studio first.' }
if (Test-Path $dst) {
    if ($PSCmdlet.ShouldProcess($dst, 'Remove OBS Local AI plugin directory')) { Remove-Item -LiteralPath $dst -Recurse -Force; Write-Host 'Uninstalled.' }
} else { Write-Host 'Plugin directory not found; nothing to remove.' }
