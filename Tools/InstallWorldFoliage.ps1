param(
    [string]$ProjectRoot = (Join-Path $PSScriptRoot '..'),
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$List,
    [switch]$VerifyOnly
)

# Installs only the small CC0 foliage set in Data/WorldFoliageDependencies.json.
# Downloaded files and generated Unreal packages are local and ignored by Git.
# Close the project in Unreal Editor before an install; verification is read-only.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($List -and $VerifyOnly) { throw 'Choose either -List or -VerifyOnly.' }

$project = (Resolve-Path -LiteralPath $ProjectRoot).Path.TrimEnd('\', '/')
$engine = (Resolve-Path -LiteralPath $EngineRoot).Path.TrimEnd('\', '/')
$uproject = Join-Path $project 'SpacePatriotUnreal.uproject'
if (-not (Test-Path -LiteralPath $uproject -PathType Leaf)) { throw "Missing project: $uproject" }
$manifest = Get-Content -LiteralPath (Join-Path $project 'Data\WorldFoliageDependencies.json') -Raw | ConvertFrom-Json

function Local-Path([string]$relative) {
    if ([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\/])\.\.([\/]|$)') {
        throw "Unsafe dependency path: $relative"
    }
    $full = [IO.Path]::GetFullPath((Join-Path $project ($relative -replace '/', '\')))
    if (-not $full.StartsWith($project + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Dependency path escaped project: $relative"
    }
    return $full
}

function File-State([string]$path, [long]$bytes, [string]$hash) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return 'MISSING' }
    $actualSize = (Get-Item -LiteralPath $path).Length
    if ($actualSize -le 4 -or ($bytes -gt 0 -and $actualSize -ne $bytes)) { return 'MISMATCH' }
    if ($hash) {
        if ((Get-FileHash -LiteralPath $path -Algorithm MD5).Hash.ToLowerInvariant() -ne $hash.ToLowerInvariant()) { return 'MISMATCH' }
    } elseif ($path.EndsWith('.uasset', [StringComparison]::OrdinalIgnoreCase)) {
        $stream = [IO.File]::OpenRead($path)
        try {
            $tag = New-Object byte[] 4
            [void]$stream.Read($tag, 0, 4)
            if ([BitConverter]::ToString($tag) -ne 'C1-83-2A-9E') { return 'MISMATCH' }
        } finally { $stream.Dispose() }
    }
    return 'OK'
}

$rows = [System.Collections.Generic.List[object]]::new()
$seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($asset in $manifest.assets) {
    if ($asset.id -notmatch '^[a-z0-9_]+$' -or $asset.folder -notmatch '^[A-Za-z0-9]+$') { throw "Invalid asset identifier: $($asset.id)" }
    foreach ($file in $asset.files) {
        if ($file.filename -notmatch '^[A-Za-z0-9_.-]+$' -or $file.md5 -notmatch '^[0-9a-fA-F]{32}$' -or [long]$file.bytes -le 4) {
            throw "Invalid source entry: $($file.filename)"
        }
        $uri = [Uri]$file.url
        if ($uri.Scheme -ne 'https' -or $uri.Host -ne 'dl.polyhaven.org') { throw "Untrusted download host: $($file.url)" }
        $relative = "SourceAssets/PolyHaven/$($asset.id)/$($file.filename)"
        if (-not $seen.Add($relative)) { throw "Duplicate dependency: $relative" }
        $rows.Add([pscustomobject]@{
            Kind = 'SOURCE'; Relative = $relative; Path = (Local-Path $relative)
            Url = $file.url; Bytes = [long]$file.bytes; Hash = $file.md5
        })
    }
    foreach ($relative in $asset.expected_packages) {
        if ($relative -notmatch ('^Content/SpacePatriot/OpenAssets/PolyHaven/Foliage/' + $asset.folder + '/[A-Za-z0-9_.-]+\.uasset$')) {
            throw "Invalid imported package path: $relative"
        }
        if (-not $seen.Add($relative)) { throw "Duplicate dependency: $relative" }
        $rows.Add([pscustomobject]@{
            Kind = 'IMPORT'; Relative = $relative; Path = (Local-Path $relative)
            Url = ''; Bytes = 0L; Hash = ''
        })
    }
}

$states = @($rows | ForEach-Object {
    [pscustomobject]@{ Row = $_; State = (File-State $_.Path $_.Bytes $_.Hash) }
})
if ($List) {
    foreach ($state in $states) { '{0,-6} {1,-8} {2}' -f $state.Row.Kind, $state.State, $state.Row.Relative }
    return
}
$mismatched = @($states | Where-Object State -eq 'MISMATCH')
if ($mismatched.Count) { throw "$($mismatched.Count) existing foliage files differ from the manifest; none were replaced." }
if ($VerifyOnly) {
    $missing = @($states | Where-Object State -eq 'MISSING')
    if ($missing.Count) { throw "$($missing.Count) foliage dependencies are missing. Run Tools/InstallWorldFoliage.ps1." }
    Write-Output "Verified $(@($states | Where-Object {$_.Row.Kind -eq 'SOURCE'}).Count) source files and $(@($states | Where-Object {$_.Row.Kind -eq 'IMPORT'}).Count) Unreal packages."
    return
}

foreach ($state in @($states | Where-Object { $_.Row.Kind -eq 'SOURCE' -and $_.State -eq 'MISSING' })) {
    $row = $state.Row
    $parent = Split-Path -Parent $row.Path
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
    $temporary = Join-Path $parent ('.' + [IO.Path]::GetFileName($row.Path) + '.' + [guid]::NewGuid().ToString('N') + '.tmp')
    try {
        Invoke-WebRequest -Uri $row.Url -OutFile $temporary -UseBasicParsing | Out-Null
        if ((File-State $temporary $row.Bytes $row.Hash) -ne 'OK') { throw "Checksum failed: $($row.Relative)" }
        if (Test-Path -LiteralPath $row.Path) { throw "Refusing to overwrite: $($row.Relative)" }
        Move-Item -LiteralPath $temporary -Destination $row.Path
        Write-Output "Downloaded $($row.Relative)"
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}

$imports = @($rows | Where-Object Kind -eq 'IMPORT')
$missingImports = @($imports | Where-Object { (File-State $_.Path 0 '') -eq 'MISSING' })
if ($missingImports.Count -gt 0) {
    if ($missingImports.Count -ne $imports.Count) {
        throw 'Partial foliage import found. The importer would overwrite an existing local asset; resolve the partial set manually.'
    }
    $dll = Local-Path 'Binaries/Win64/UnrealEditor-SpacePatriotUnreal.dll'
    if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) {
        $build = Join-Path $engine 'Engine\Build\BatchFiles\Build.bat'
        if (-not (Test-Path -LiteralPath $build -PathType Leaf)) { throw "Missing Unreal build tool: $build" }
        & $build SpacePatriotUnrealEditor Win64 Development "-Project=$uproject" -WaitMutex -NoHotReloadFromIDE
        if ($LASTEXITCODE -ne 0) { throw "Editor target build failed: $LASTEXITCODE" }
    }
    $cmd = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $importer = Local-Path 'Tools/ImportWorldFoliage.py'
    if (-not (Test-Path -LiteralPath $cmd -PathType Leaf)) { throw "Missing Unreal commandlet: $cmd" }
    Write-Output 'Importing three foliage assets with UnrealEditor-Cmd (no game window)...'
    $args = @(('"' + $uproject + '"'), '/Engine/Maps/Entry', ('-ExecutePythonScript="' + $importer + '"'), '-unattended', '-nullrhi', '-nosplash', '-nop4', '-NoSound')
    $process = Start-Process -FilePath $cmd -ArgumentList $args -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -ne 0) { throw "Foliage importer failed: $($process.ExitCode)" }
}
$left = @($rows | Where-Object { (File-State $_.Path $_.Bytes $_.Hash) -ne 'OK' })
if ($left.Count) { throw "Foliage installation incomplete: $($left.Count) files unresolved." }
Write-Output "Ready: $(@($rows | Where-Object Kind -eq 'SOURCE').Count) downloaded files and $($imports.Count) Unreal packages."
