[CmdletBinding(SupportsShouldProcess=$true)]
param(
    # OBS 32 and older only look inside <plugin>\bin\64bit. OBS 33 looks for
    # <plugin>\<plugin>.dll first and still accepts the legacy layout, so the
    # default here is the forward-compatible one.
    [switch] $LegacyLayout,
    [string] $PluginRoot = (Join-Path $env:ProgramData 'obs-studio\plugins')
)
$ErrorActionPreference = 'Stop'

$name = 'obs-local-ai'
$src = $PSScriptRoot
$dst = Join-Path $PluginRoot $name

# Where previous versions of this script (wrongly) installed to: OBS 33 does not
# scan the per-user plugins folder unless OBS_PLUGINS_PATH is set, so a copy
# there is invisible to OBS.
$staleDst = Join-Path $env:APPDATA "obs-studio\plugins\$name"

if (Get-Process -Name 'obs64', 'obs32' -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before installing the plugin.'
}

foreach ($f in @('bin\64bit\obs-local-ai.dll', 'data\face_detection_yunet_2023mar.onnx', 'data\locale\en-US.ini')) {
    if (-not (Test-Path (Join-Path $src $f))) { throw "Package is incomplete: $f is missing. Build the plugin first; the source ZIP is not an installer." }
}

if (-not (Get-ChildItem (Join-Path $src 'bin\64bit') -Filter 'opencv*.dll' -ErrorAction SilentlyContinue)) {
    throw 'OpenCV runtime DLLs are missing from the package.'
}

if (Test-Path $dst) {
    throw "Destination already exists: $dst. Back it up or remove it first (or run Uninstall.ps1)."
}

if (Test-Path $staleDst) {
    if ($PSCmdlet.ShouldProcess($staleDst, 'Remove unusable per-user copy')) {
        Remove-Item $staleDst -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "Removed an unusable per-user copy from $staleDst (OBS 33 does not scan that folder)."
    }
}

if ($PSCmdlet.ShouldProcess($dst, 'Install OBS Local AI Auto Frame')) {
    $binaryDir = if ($LegacyLayout) { Join-Path $dst 'bin\64bit' } else { $dst }

    New-Item -ItemType Directory -Path $binaryDir -Force | Out-Null
    try {
        # The DLL has to be named after its folder: OBS resolves
        # <plugin folder>\<plugin folder>.dll.
        Copy-Item (Join-Path $src 'bin\64bit\obs-local-ai.dll') $binaryDir -Force
        Get-ChildItem (Join-Path $src 'bin\64bit') -Filter 'opencv*.dll' |
            Copy-Item -Destination $binaryDir -Force
        Copy-Item (Join-Path $src 'data') $dst -Recurse -Force
    }
    catch {
        Remove-Item $dst -Recurse -Force -ErrorAction SilentlyContinue
        throw "Could not write to $dst ($($_.Exception.Message)). Run PowerShell as administrator, or pass -PluginRoot with a folder OBS scans."
    }

    if (-not (Test-Path (Join-Path $binaryDir 'obs-local-ai.dll'))) {
        Remove-Item $dst -Recurse -Force -ErrorAction SilentlyContinue
        throw "Installation failed: $binaryDir\obs-local-ai.dll was not written."
    }

    $layout = if ($LegacyLayout) { 'legacy bin\64bit layout' } else { 'OBS 33 layout' }
    Write-Host "Installed to $dst ($layout)."
    Write-Host 'Start OBS and add "Local AI Auto Frame" in the Filters dialog of a camera source.'
    Write-Host "If it does not appear, check Help -> Log Files -> View Current Log for 'Local AI'."
}
