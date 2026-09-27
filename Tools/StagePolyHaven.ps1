param(
    [string]$Destination = (Join-Path $PSScriptRoot '..\SourceAssets\PolyHaven')
)

$ErrorActionPreference = 'Stop'

# The Poly Haven API supplies direct URLs and MD5 checksums. Keep each source
# asset intact so the Unreal import can be reproduced without Fab accounts.
$selection = @(
    @{ Id = 'hangar_concrete_floor'; Author = 'Dimitrios Savva'; NativeWidthCm = 200; Files = @(
        @{ Kind = 'Diffuse'; Resolution = '2k'; Format = 'jpg' },
        @{ Kind = 'nor_dx'; Resolution = '2k'; Format = 'png' },
        @{ Kind = 'Rough'; Resolution = '2k'; Format = 'jpg' },
        @{ Kind = 'AO'; Resolution = '2k'; Format = 'jpg' }
    ) },
    @{ Id = 'boulder_01'; Author = 'Rico Cilliers'; NativeWidthCm = $null; Files = @(
        @{ Kind = 'fbx'; Resolution = '1k'; Format = 'fbx' },
        @{ Kind = 'Diffuse'; Resolution = '1k'; Format = 'jpg' },
        @{ Kind = 'nor_dx'; Resolution = '1k'; Format = 'png' },
        @{ Kind = 'Rough'; Resolution = '1k'; Format = 'jpg' }
    ) },
    @{ Id = 'plastic_crate_02'; Author = 'Fabi_G'; NativeWidthCm = 50.5; Files = @(
        @{ Kind = 'fbx'; Resolution = '1k'; Format = 'fbx' },
        @{ Kind = 'Diffuse'; Resolution = '1k'; Format = 'jpg' },
        @{ Kind = 'nor_dx'; Resolution = '1k'; Format = 'png' },
        @{ Kind = 'Rough'; Resolution = '1k'; Format = 'jpg' },
        @{ Kind = 'opacity'; Resolution = '1k'; Format = 'png' }
    ) },
    @{ Id = 'rocks_ground_02'; Author = 'Rob Tuytel'; NativeWidthCm = 200; Files = @(
        @{ Kind = 'Diffuse'; Resolution = '2k'; Format = 'jpg' },
        @{ Kind = 'nor_dx'; Resolution = '1k'; Format = 'png' },
        @{ Kind = 'Rough'; Resolution = '1k'; Format = 'jpg' },
        @{ Kind = 'AO'; Resolution = '1k'; Format = 'jpg' }
    ) },
    @{ Id = 'grass_ground'; Author = 'Charlotte Baglioni'; NativeWidthCm = 251; Files = @(
        @{ Kind = 'Diffuse'; Resolution = '2k'; Format = 'jpg' },
        @{ Kind = 'nor_dx'; Resolution = '1k'; Format = 'png' },
        @{ Kind = 'Rough'; Resolution = '1k'; Format = 'jpg' },
        @{ Kind = 'AO'; Resolution = '1k'; Format = 'jpg' }
    ) },
    @{ Id = 'red_sand'; Author = 'Rohit Seervi'; NativeWidthCm = 300; Files = @(
        @{ Kind = 'Diffuse'; Resolution = '2k'; Format = 'jpg' },
        @{ Kind = 'nor_dx'; Resolution = '1k'; Format = 'png' },
        @{ Kind = 'Rough'; Resolution = '1k'; Format = 'jpg' },
        @{ Kind = 'AO'; Resolution = '1k'; Format = 'jpg' }
    ) },
    @{ Id = 'namaqualand_boulder_03'; Author = 'Dario Barresi; Jenelle van Heerden'; NativeWidthCm = 307; Files = @(
        @{ Kind = 'fbx'; Resolution = '1k'; Format = 'fbx' },
        @{ Kind = 'Diffuse'; Resolution = '1k'; Format = 'jpg' },
        @{ Kind = 'nor_dx'; Resolution = '1k'; Format = 'png' },
        @{ Kind = 'Rough'; Resolution = '1k'; Format = 'jpg' }
    ) },
    @{ Id = 'namaqualand_boulder_05'; Author = 'Dario Barresi; Jenelle van Heerden'; NativeWidthCm = 136; Files = @(
        @{ Kind = 'fbx'; Resolution = '1k'; Format = 'fbx' },
        @{ Kind = 'Diffuse'; Resolution = '1k'; Format = 'jpg' },
        @{ Kind = 'nor_dx'; Resolution = '1k'; Format = 'png' },
        @{ Kind = 'Rough'; Resolution = '1k'; Format = 'jpg' }
    ) }
)

New-Item -ItemType Directory -Path $Destination -Force | Out-Null
$manifest = @()
foreach ($asset in $selection) {
    $id = $asset.Id
    $metadata = Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$id"
    $assetDirectory = Join-Path $Destination $id
    New-Item -ItemType Directory -Path $assetDirectory -Force | Out-Null
    foreach ($request in $asset.Files) {
        $record = $metadata.($request.Kind).($request.Resolution).($request.Format)
        if (-not $record -or -not $record.url) {
            throw "Poly Haven has no $($request.Kind)/$($request.Resolution)/$($request.Format) file for $id"
        }
        $filename = [IO.Path]::GetFileName(([Uri]$record.url).AbsolutePath)
        $target = Join-Path $assetDirectory $filename
        if (-not (Test-Path -LiteralPath $target)) {
            Invoke-WebRequest -Uri $record.url -OutFile $target
        }
        $hash = (Get-FileHash -LiteralPath $target -Algorithm MD5).Hash.ToLowerInvariant()
        if ($hash -ne $record.md5.ToLowerInvariant()) {
            throw "MD5 mismatch for $target. Expected $($record.md5), got $hash"
        }
        $manifest += [ordered]@{
            asset = $id
            author = $asset.Author
            asset_page = "https://polyhaven.com/a/$id"
            file = $filename
            resolution = $request.Resolution
            format = $request.Format
            kind = $request.Kind
            native_width_cm = $asset.NativeWidthCm
            source_url = $record.url
            bytes = (Get-Item -LiteralPath $target).Length
            md5 = $hash
            license = 'CC0 (https://polyhaven.com/license)'
        }
        Write-Output "VERIFIED $target ($((Get-Item -LiteralPath $target).Length) bytes)"
    }
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $Destination 'manifest.json') -Encoding UTF8
Write-Output "MANIFEST $(Join-Path $Destination 'manifest.json')"
