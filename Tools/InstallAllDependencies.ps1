param(
    [string]$ProjectRoot = (Join-Path $PSScriptRoot '..'),
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$List,
    [switch]$VerifyOnly
)

# Local-only packs used by the cohesive port map. The manifests pin sources
# and checksums; no third-party asset binaries belong in this Git repository.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($List -and $VerifyOnly) { throw 'Choose either -List or -VerifyOnly.' }

$root = (Resolve-Path -LiteralPath $ProjectRoot).Path
$installers = @(
    (Join-Path $PSScriptRoot 'InstallLocalDependencies.ps1'),
    (Join-Path $PSScriptRoot 'InstallWorldFoliage.ps1'),
    (Join-Path $PSScriptRoot 'InstallViktorHeroShip.ps1')
)
foreach ($installer in $installers) {
    if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
        throw "Missing dependency installer: $installer"
    }
    Write-Output "Checking $([IO.Path]::GetFileName($installer))"
    if ($List) {
        & $installer -ProjectRoot $root -EngineRoot $EngineRoot -List
    } elseif ($VerifyOnly) {
        & $installer -ProjectRoot $root -EngineRoot $EngineRoot -VerifyOnly
    } else {
        & $installer -ProjectRoot $root -EngineRoot $EngineRoot
    }
    if (-not $?) { throw "Dependency installer failed: $installer" }
}
Write-Output 'All local Space Patriot dependencies are ready.'
