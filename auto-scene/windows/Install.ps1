[CmdletBinding(SupportsShouldProcess=$true)]
param(
    # OBS 33 finds <plugin>\<plugin>.dll and still accepts the older
    # <plugin>\bin\64bit layout. The default here is the forward-compatible one.
    [switch] $LegacyLayout,
    [string] $PluginRoot = (Join-Path $env:ProgramData 'obs-studio\plugins')
)
$ErrorActionPreference = 'Stop'

$name = 'obs-auto-scene'
$src  = $PSScriptRoot
$dst  = Join-Path $PluginRoot $name

# OBS 33 does not scan the per-user plugins folder unless OBS_PLUGINS_PATH is
# set, so a copy there would be invisible.
$staleDst = Join-Path $env:APPDATA "obs-studio\plugins\$name"

if (Get-Process -Name 'obs64', 'obs32' -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before installing the plugin.'
}

foreach ($f in @('bin\64bit\obs-auto-scene.dll', 'data\locale\en-US.ini')) {
    if (-not (Test-Path (Join-Path $src $f))) {
        throw "Package is incomplete: $f is missing. Download the build artifact named obs-auto-scene-windows-x64, not the source ZIP."
    }
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

if ($PSCmdlet.ShouldProcess($dst, 'Install Auto Scene Adjust')) {
    $binaryDir = if ($LegacyLayout) { Join-Path $dst 'bin\64bit' } else { $dst }

    New-Item -ItemType Directory -Path $binaryDir -Force | Out-Null
    try {
        # The DLL has to be named after its folder: OBS resolves
        # <plugin folder>\<plugin folder>.dll.
        Copy-Item (Join-Path $src 'bin\64bit\obs-auto-scene.dll') $binaryDir -Force
        Copy-Item (Join-Path $src 'data') $dst -Recurse -Force
    }
    catch {
        Remove-Item $dst -Recurse -Force -ErrorAction SilentlyContinue
        throw "Could not write to $dst ($($_.Exception.Message)). Run PowerShell as administrator, or pass -PluginRoot with a folder OBS scans."
    }

    if (-not (Test-Path (Join-Path $binaryDir 'obs-auto-scene.dll'))) {
        throw 'The DLL did not end up where OBS will look for it.'
    }
}

Write-Host ''
Write-Host "Auto Scene Adjust installed to $dst"
Write-Host ''
Write-Host 'Next steps:'
Write-Host '  1. Start OBS Studio 33 or newer.'
Write-Host '  2. Open the dock: Docks -> Auto Scene Adjust.'
Write-Host '  3. Press "Fit this scene" once - every source OBS lets the plugin move'
Write-Host '     is fitted to the canvas, and kept fitted from then on.'
Write-Host '  4. Try Grid, Side by side and Main + cam to arrange a scene, and Undo'
Write-Host '     when a preset is not what you wanted.'
Write-Host ''
Write-Host 'Logs: Help -> Log Files -> View Current Log, search for "obs-auto-scene".'
