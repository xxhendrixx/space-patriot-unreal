param(
    [string]$ProjectRoot = (Join-Path $PSScriptRoot '..'),
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$List,
    [switch]$VerifyOnly
)

# Installs the selected CC BY exterior locally. The archive and generated
# packages are ignored by Git; collaborators fetch them for themselves.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($List -and $VerifyOnly) { throw 'Choose either -List or -VerifyOnly.' }

$project = (Resolve-Path -LiteralPath $ProjectRoot).Path.TrimEnd('\', '/')
$engine = (Resolve-Path -LiteralPath $EngineRoot).Path.TrimEnd('\', '/')
$manifest = Get-Content -LiteralPath (Join-Path $project 'Data\ViktorShipsDependency.json') -Raw | ConvertFrom-Json
$projectFile = Join-Path $project 'SpacePatriotUnreal.uproject'
$archive = Join-Path $project 'SourceAssets\ExternalShips\ViktorHahn\spaceships2.zip'
$contentRoot = Join-Path $project 'Content\SpacePatriot\OpenAssets\ViktorShips'
$contentPrefix = [IO.Path]::GetFullPath($contentRoot) + [IO.Path]::DirectorySeparatorChar

if ($manifest.archive_url -ne 'https://opengameart.org/sites/default/files/spaceships2.zip' -or
    $manifest.archive_sha256 -notmatch '^[a-fA-F0-9]{64}$') {
    throw 'Unexpected ship dependency source or hash.'
}
if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) { throw "Project not found: $projectFile" }

function Get-ArchiveState {
    if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) { return 'MISSING' }
    if ((Get-Item -LiteralPath $archive).Length -ne [long]$manifest.archive_bytes) { return 'MISMATCH' }
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -ne $manifest.archive_sha256) { return 'MISMATCH' }
    return 'OK'
}

function Get-PackageState([string]$relative) {
    if ($relative -notmatch '^Content/SpacePatriot/OpenAssets/ViktorShips/[A-Za-z0-9_]+\.uasset$') {
        throw "Unsafe package entry: $relative"
    }
    $path = [IO.Path]::GetFullPath((Join-Path $project ($relative -replace '/', '\')))
    if (-not $path.StartsWith($contentPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Package escapes its local directory: $relative"
    }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return 'MISSING' }
    $stream = [IO.File]::OpenRead($path)
    try {
        if ($stream.Length -lt 4) { return 'MISMATCH' }
        $tag = New-Object byte[] 4
        [void]$stream.Read($tag, 0, 4)
        if ([BitConverter]::ToString($tag) -ne 'C1-83-2A-9E') { return 'MISMATCH' }
    } finally { $stream.Dispose() }
    return 'OK'
}

$archiveState = Get-ArchiveState
$packages = @($manifest.hero_local_asset_files)
$packageStates = @($packages | ForEach-Object { [pscustomobject]@{ Relative = $_; State = (Get-PackageState $_) } })
if ($List) {
    "ARCHIVE $archiveState SourceAssets/ExternalShips/ViktorHahn/spaceships2.zip"
    foreach ($row in $packageStates) { "PACKAGE $($row.State) $($row.Relative)" }
    return
}
if ($archiveState -eq 'MISMATCH' -or @($packageStates | Where-Object State -eq 'MISMATCH').Count -gt 0) {
    throw 'A local ship dependency differs from the manifest; refusing to overwrite it.'
}
if ($VerifyOnly) {
    if ($archiveState -ne 'OK' -or @($packageStates | Where-Object State -ne 'OK').Count -gt 0) {
        throw 'The local hero-ship dependency is incomplete.'
    }
    'Verified the downloaded ship archive and all six imported Unreal packages.'
    return
}

if ($archiveState -eq 'MISSING') {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $archive) | Out-Null
    $temporary = $archive + '.' + [guid]::NewGuid().ToString('N') + '.download'
    try {
        Invoke-WebRequest -Uri $manifest.archive_url -OutFile $temporary -UseBasicParsing | Out-Null
        if ((Get-Item -LiteralPath $temporary).Length -ne [long]$manifest.archive_bytes -or
            (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash.ToLowerInvariant() -ne $manifest.archive_sha256) {
            throw 'Downloaded ship archive failed size or SHA256 verification.'
        }
        Move-Item -LiteralPath $temporary -Destination $archive
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}

$missing = @($packageStates | Where-Object State -eq 'MISSING')
if ($missing.Count -gt 0) {
    if ($missing.Count -ne $packages.Count) {
        throw 'Partial ship import detected; refusing to overwrite any existing Unreal package.'
    }
    $editorDll = Join-Path $project 'Binaries\Win64\UnrealEditor-SpacePatriotUnreal.dll'
    if (-not (Test-Path -LiteralPath $editorDll -PathType Leaf)) {
        $buildTool = Join-Path $engine 'Engine\Build\BatchFiles\Build.bat'
        & $buildTool SpacePatriotUnrealEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE
        if ($LASTEXITCODE -ne 0) { throw "Unreal Editor build failed: $LASTEXITCODE" }
    }
    $editorCmd = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $script = Join-Path $project 'Tools\ImportViktorHeroShip.py'
    $arguments = @(
        ('"' + $projectFile + '"'), '/Engine/Maps/Entry',
        ('-ExecutePythonScript="' + $script + '"'),
        '-unattended', '-nullrhi', '-nosplash', '-nop4', '-NoSound'
    )
    $process = Start-Process -FilePath $editorCmd -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -ne 0) { throw "Unreal ship import failed: $($process.ExitCode)" }
}
if ((Get-ArchiveState) -ne 'OK' -or @($packages | Where-Object { (Get-PackageState $_) -ne 'OK' }).Count -gt 0) {
    throw 'Ship dependency installation did not produce all expected packages.'
}
'Ready: verified CC BY exterior ship locally. The pack binaries are excluded from Git.'
