[CmdletBinding(SupportsShouldProcess=$true)]
param()
$ErrorActionPreference = 'Stop'
$src = $PSScriptRoot
$dst = Join-Path $env:APPDATA 'obs-studio\plugins\obs-local-ai'
if (Get-Process -Name 'obs64','obs32' -ErrorAction SilentlyContinue) { throw 'Close OBS Studio before installing the plugin.' }
foreach ($f in @('bin\64bit\obs-local-ai.dll','data\face_detection_yunet_2023mar.onnx','data\locale\en-US.ini')) {
    if (-not (Test-Path (Join-Path $src $f))) { throw "Package is incomplete: $f is missing. Build the plugin first; the source ZIP is not an installer." }
}
if (-not (Get-ChildItem (Join-Path $src 'bin\64bit') -Filter 'opencv*.dll' -ErrorAction SilentlyContinue)) { throw 'OpenCV runtime DLLs are missing from the package.' }
if (Test-Path $dst) { throw "Destination already exists: $dst. Back up or remove it before installing." }
if ($PSCmdlet.ShouldProcess($dst, 'Install OBS Local AI')) {
    New-Item -ItemType Directory -Path $dst -Force | Out-Null
    try {
        Copy-Item (Join-Path $src 'bin') $dst -Recurse
        Copy-Item (Join-Path $src 'data') $dst -Recurse
    } catch { Remove-Item $dst -Recurse -Force -ErrorAction SilentlyContinue; throw }
    Write-Host "Installed to $dst. Start OBS and add Local AI Auto Frame to a compatible camera source."
}
