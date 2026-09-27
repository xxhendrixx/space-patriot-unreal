param(
    [string]$ProjectRoot = (Join-Path $PSScriptRoot '..'),
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$List,
    [switch]$VerifyOnly
)

# Restores only the assets in Data/ExternalRuntimeContentManifest.json. Epic
# template files come from the local UE 5.8 installation; Poly Haven sources
# come from the exact URLs and checksums in SourceAssets/PolyHaven/manifest.json.
# This never opens a playable game window.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($List -and $VerifyOnly) {
    throw 'Choose either -List or -VerifyOnly.'
}

$project = (Resolve-Path -LiteralPath $ProjectRoot).Path.TrimEnd('\', '/')
$engine = (Resolve-Path -LiteralPath $EngineRoot).Path.TrimEnd('\', '/')
$projectFile = Join-Path $project 'SpacePatriotUnreal.uproject'
if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "Unreal project not found: $projectFile"
}

function Resolve-WithinRoot {
    param([string]$Root, [string]$Relative)
    if ([string]::IsNullOrWhiteSpace($Relative) -or
        [IO.Path]::IsPathRooted($Relative) -or
        $Relative -match '(^|[\\/])\.\.([\\/]|$)') {
        throw "Unsafe manifest path: $Relative"
    }
    $resolved = [IO.Path]::GetFullPath((Join-Path $Root ($Relative -replace '/', '\')))
    $prefix = $Root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Manifest path escapes its root: $Relative"
    }
    return $resolved
}

function Test-File {
    param([string]$Path, [long]$Bytes, [string]$Algorithm, [string]$ExpectedHash)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return 'MISSING' }
    $size = (Get-Item -LiteralPath $Path).Length
    if ($size -le 0 -or ($Bytes -gt 0 -and $size -ne $Bytes)) { return 'MISMATCH' }
    # Imported Unreal assets have no stable cross-machine hash. Check their
    # package tag so a Git LFS pointer or truncated file cannot pass as one.
    if (-not $ExpectedHash -and $Path.EndsWith('.uasset', [StringComparison]::OrdinalIgnoreCase)) {
        if ($size -lt 4) { return 'MISMATCH' }
        $stream = [IO.File]::OpenRead($Path)
        try {
            $tag = New-Object byte[] 4
            [void]$stream.Read($tag, 0, 4)
            if ([BitConverter]::ToString($tag) -ne 'C1-83-2A-9E') { return 'MISMATCH' }
        } finally {
            $stream.Dispose()
        }
    }
    if ($ExpectedHash) {
        $actual = (Get-FileHash -LiteralPath $Path -Algorithm $Algorithm).Hash.ToLowerInvariant()
        if ($actual -ne $ExpectedHash.ToLowerInvariant()) { return 'MISMATCH' }
    }
    return 'OK'
}

function Copy-VerifiedFile {
    param([string]$Source, [string]$Destination, [long]$Bytes, [string]$Hash)
    if ((Test-File -Path $Source -Bytes $Bytes -Algorithm SHA256 -ExpectedHash $Hash) -ne 'OK') {
        throw "Installed engine file is missing or differs from the manifest: $Source"
    }
    $parent = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
    $temporary = Join-Path $parent ('.' + [IO.Path]::GetFileName($Destination) + '.' + [guid]::NewGuid().ToString('N') + '.tmp')
    try {
        Copy-Item -LiteralPath $Source -Destination $temporary
        if ((Test-File -Path $temporary -Bytes $Bytes -Algorithm SHA256 -ExpectedHash $Hash) -ne 'OK') {
            throw "Copied file failed checksum verification: $Destination"
        }
        if (Test-Path -LiteralPath $Destination) {
            throw "Refusing to replace an existing file: $Destination"
        }
        Move-Item -LiteralPath $temporary -Destination $Destination
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}

function Download-VerifiedFile {
    param([string]$Url, [string]$Destination, [long]$Bytes, [string]$Hash)
    $uri = [Uri]$Url
    if ($uri.Scheme -ne 'https' -or $uri.Host -ne 'dl.polyhaven.org') {
        throw "Unexpected Poly Haven download host: $Url"
    }
    $parent = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
    $temporary = Join-Path $parent ('.' + [IO.Path]::GetFileName($Destination) + '.' + [guid]::NewGuid().ToString('N') + '.tmp')
    try {
        Invoke-WebRequest -Uri $uri -OutFile $temporary -UseBasicParsing | Out-Null
        if ((Test-File -Path $temporary -Bytes $Bytes -Algorithm MD5 -ExpectedHash $Hash) -ne 'OK') {
            throw "Downloaded file failed checksum verification: $Destination"
        }
        if (Test-Path -LiteralPath $Destination) {
            throw "Refusing to replace an existing file: $Destination"
        }
        Move-Item -LiteralPath $temporary -Destination $Destination
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}

$manifestPath = Resolve-WithinRoot $project 'Data/ExternalRuntimeContentManifest.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "Missing dependency manifest: $manifestPath" }
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$rawManifestPath = Resolve-WithinRoot $project $manifest.poly_haven_download_manifest
if (-not (Test-Path -LiteralPath $rawManifestPath -PathType Leaf)) { throw "Missing Poly Haven manifest: $rawManifestPath" }
$rawManifest = @(Get-Content -LiteralPath $rawManifestPath -Raw | ConvertFrom-Json)
$epic = @($manifest.epic_template_required)
$imported = @($manifest.poly_haven_imported_required)
$rows = [System.Collections.Generic.List[object]]::new()
$seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)

foreach ($item in $epic) {
    if ($item.destination -notmatch '^Content/.+\.uasset$' -or $item.source_relative_to_engine -notmatch '^Templates/.+\.uasset$' -or $item.sha256 -notmatch '^[a-fA-F0-9]{64}$') {
        throw "Invalid Epic template manifest entry: $($item.destination)"
    }
    if (-not $seen.Add($item.destination)) { throw "Duplicate destination: $($item.destination)" }
    $rows.Add([pscustomobject]@{
        Kind = 'EPIC'; Relative = $item.destination
        Path = (Resolve-WithinRoot $project $item.destination)
        Source = (Resolve-WithinRoot $engine $item.source_relative_to_engine)
        Bytes = [long]$item.bytes; Hash = $item.sha256
    })
}
foreach ($item in $rawManifest) {
    if ($item.asset -notmatch '^[a-z0-9_]+$' -or $item.file -notmatch '^[A-Za-z0-9_.-]+$' -or $item.md5 -notmatch '^[a-fA-F0-9]{32}$') {
        throw "Invalid Poly Haven manifest entry: $($item.file)"
    }
    $relative = 'SourceAssets/PolyHaven/' + $item.asset + '/' + $item.file
    if (-not $seen.Add($relative)) { throw "Duplicate destination: $relative" }
    $rows.Add([pscustomobject]@{
        Kind = 'POLY_SOURCE'; Relative = $relative
        Path = (Resolve-WithinRoot $project $relative)
        Source = $item.source_url
        Bytes = [long]$item.bytes; Hash = $item.md5
    })
}
foreach ($item in $imported) {
    if ($item.destination -notmatch '^Content/SpacePatriot/OpenAssets/PolyHaven/.+\.uasset$') {
        throw "Invalid Poly Haven import manifest entry: $($item.destination)"
    }
    if (-not $seen.Add($item.destination)) { throw "Duplicate destination: $($item.destination)" }
    $rows.Add([pscustomobject]@{
        Kind = 'POLY_IMPORT'; Relative = $item.destination
        Path = (Resolve-WithinRoot $project $item.destination)
        Source = ''; Bytes = 0L; Hash = ''
    })
}

function Get-RowState {
    param($Row)
    if ($Row.Kind -eq 'EPIC') { return Test-File $Row.Path $Row.Bytes SHA256 $Row.Hash }
    if ($Row.Kind -eq 'POLY_SOURCE') { return Test-File $Row.Path $Row.Bytes MD5 $Row.Hash }
    return Test-File $Row.Path 0 '' ''
}

$stateRows = foreach ($row in $rows) {
    [pscustomobject]@{ Kind = $row.Kind; Relative = $row.Relative; State = (Get-RowState $row); Row = $row }
}
if ($List) {
    foreach ($row in $stateRows) { '{0,-11} {1,-8} {2}' -f $row.Kind, $row.State, $row.Relative }
    'Listed {0} Epic, {1} Poly Haven source, and {2} imported assets.' -f $epic.Count, $rawManifest.Count, $imported.Count
    return
}

$problems = @($stateRows | Where-Object State -eq 'MISMATCH')
if ($problems.Count -gt 0) {
    foreach ($problem in $problems) { Write-Error "Existing file differs from manifest: $($problem.Relative)" -ErrorAction Continue }
    throw "Found $($problems.Count) mismatched files; no existing asset was replaced."
}
if ($VerifyOnly) {
    $missing = @($stateRows | Where-Object State -eq 'MISSING')
    foreach ($item in $missing) { Write-Output "MISSING $($item.Kind) $($item.Relative)" }
    if ($missing.Count -gt 0) { throw "Missing $($missing.Count) local dependencies." }
    Write-Output "Verified $($epic.Count) Epic, $($rawManifest.Count) Poly Haven source, and $($imported.Count) imported assets."
    return
}

# Validate all missing Epic sources before writing any destination file.
$missingEpic = @($stateRows | Where-Object { $_.Kind -eq 'EPIC' -and $_.State -eq 'MISSING' })
foreach ($entry in $missingEpic) {
    $row = $entry.Row
    if ((Test-File $row.Source $row.Bytes SHA256 $row.Hash) -ne 'OK') {
        throw "The local UE 5.8 template does not match the manifest: $($row.Source)"
    }
}
foreach ($entry in $missingEpic) {
    $row = $entry.Row
    Copy-VerifiedFile $row.Source $row.Path $row.Bytes $row.Hash
    Write-Output "Installed Epic template: $($row.Relative)"
}
foreach ($entry in @($stateRows | Where-Object { $_.Kind -eq 'POLY_SOURCE' -and $_.State -eq 'MISSING' })) {
    $row = $entry.Row
    Download-VerifiedFile $row.Source $row.Path $row.Bytes $row.Hash
    Write-Output "Downloaded Poly Haven source: $($row.Relative)"
}

$missingImports = @($rows | Where-Object { $_.Kind -eq 'POLY_IMPORT' -and (Get-RowState $_) -eq 'MISSING' })
if ($missingImports.Count -gt 0) {
    # A fresh clone has source modules but no project DLLs. UnrealEditor-Cmd
    # cannot run the Python importers until the Editor target is compiled.
    $buildTool = Resolve-WithinRoot $engine 'Engine/Build/BatchFiles/Build.bat'
    if (-not (Test-Path -LiteralPath $buildTool -PathType Leaf)) { throw "Unreal build tool not found: $buildTool" }
    Write-Output 'Building SpacePatriotUnrealEditor for first-time imports...'
    & $buildTool SpacePatriotUnrealEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw "Unreal Editor target build failed with exit code $LASTEXITCODE." }
}

$editorCommand = Resolve-WithinRoot $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$importGroups = @(
    @{ Name = 'base'; Script = 'Tools/ImportPolyHaven.py'; Pattern = '/(Boulder01|HangarConcrete|PlasticCrate02)/' },
    @{ Name = 'rocks'; Script = 'Tools/ImportPolyHavenRocks.py'; Pattern = '/NamaqualandBoulder(03|05)/' },
    @{ Name = 'terrain'; Script = 'Tools/ImportPolyHavenTerrain.py'; Pattern = '/Terrain/' }
)
foreach ($group in $importGroups) {
    $members = @($rows | Where-Object { $_.Kind -eq 'POLY_IMPORT' -and $_.Relative -match $group.Pattern })
    $missing = @($members | Where-Object { (Get-RowState $_) -eq 'MISSING' })
    if ($missing.Count -eq 0) { continue }
    if ($missing.Count -ne $members.Count) {
        throw "Partial $($group.Name) Poly Haven import; the importer would overwrite existing assets. Restore this group manually before rerunning."
    }
    if (-not (Test-Path -LiteralPath $editorCommand -PathType Leaf)) { throw "Unreal commandlet not found: $editorCommand" }
    $scriptPath = Resolve-WithinRoot $project $group.Script
    if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) { throw "Missing importer: $scriptPath" }
    Write-Output "Importing $($group.Name) Poly Haven assets with UnrealEditor-Cmd (no game window)..."
    $arguments = @(
        ('"' + $projectFile + '"'),
        '/Engine/Maps/Entry',
        ('-ExecutePythonScript="' + $scriptPath + '"'),
        '-unattended', '-nullrhi', '-nosplash', '-nop4', '-NoSound'
    )
    $process = Start-Process -FilePath $editorCommand -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -ne 0) { throw "Poly Haven $($group.Name) import failed with exit code $($process.ExitCode)." }
    foreach ($item in $members) {
        if ((Get-RowState $item) -ne 'OK') { throw "Importer did not create required asset: $($item.Relative)" }
    }
}

$remaining = @($rows | Where-Object { (Get-RowState $_) -ne 'OK' })
if ($remaining.Count -gt 0) { throw "Installation left $($remaining.Count) dependencies unresolved." }
Write-Output "Ready: $($epic.Count) Epic, $($rawManifest.Count) Poly Haven source, and $($imported.Count) imported assets verified."
