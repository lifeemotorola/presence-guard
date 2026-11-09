# OnAirTwin for OBS Studio -- Copyright (C) 2026 Emmanuel Suah, GPL-2.0-or-later.
# Fetch an official GitHub release asset via the API and verify it before use.
param(
    [Parameter(Mandatory=$true)] [string] $Repository,
    [Parameter(Mandatory=$true)] [string] $Tag,
    [Parameter(Mandatory=$true)] [string] $AssetName,
    [Parameter(Mandatory=$true)] [string] $ExpectedHash,
    [Parameter(Mandatory=$true)] [string] $Destination
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$ExpectedHash = $ExpectedHash.ToLowerInvariant()
$parent = Split-Path -Parent $Destination
if ($parent) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
if ((Test-Path $Destination) -and ((Get-FileHash $Destination -Algorithm SHA256).Hash.ToLowerInvariant() -eq $ExpectedHash)) {
    Write-Host "Verified cached $AssetName (SHA-256 $ExpectedHash)."
    return
}
$headers = @{ Accept = 'application/vnd.github+json'; 'X-GitHub-Api-Version' = '2022-11-28' }
if ($env:GH_TOKEN) { $headers.Authorization = "Bearer $env:GH_TOKEN" }
$release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repository/releases/tags/$Tag" -Headers $headers
$asset = $release.assets | Where-Object { $_.name -eq $AssetName } | Select-Object -First 1
if (!$asset) { throw "Official release asset $AssetName not found." }
if ($asset.digest -and $asset.digest -ne "sha256:$ExpectedHash") { throw "Release metadata disagrees with the pinned checksum for $AssetName." }
$headers.Accept = 'application/octet-stream'
for ($attempt = 1; $attempt -le 3; $attempt++) {
    try {
        Invoke-WebRequest -Uri $asset.url -Headers $headers -OutFile "$Destination.part"
        $actual = (Get-FileHash "$Destination.part" -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $ExpectedHash) { throw "Checksum mismatch for ${AssetName}: $actual" }
        Move-Item "$Destination.part" $Destination -Force
        Write-Host "Verified official $AssetName (SHA-256 $ExpectedHash)."
        return
    }
    catch {
        Remove-Item "$Destination.part" -Force -ErrorAction SilentlyContinue
        if ($attempt -eq 3) { throw }
        Write-Warning "Download of $AssetName attempt $attempt failed; retrying."
        Start-Sleep -Seconds 10
    }
}
