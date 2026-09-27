# Local foliage for playable worlds

The 1K foliage set adds three real-world reference plants as temporary
vegetation on solid desert and temperate worlds. Runtime dressing needs this
set for those biomes. Its source files and imported Unreal
packages are local dependencies, not Git payloads. All download URLs, byte
counts, and MD5 checksums are pinned in
`Data/WorldFoliageDependencies.json`.

With Unreal Engine 5.8 installed, close this project in the Editor and run
from the project root:

```powershell
.\Tools\InstallWorldFoliage.ps1 -List
.\Tools\InstallWorldFoliage.ps1
.\Tools\InstallWorldFoliage.ps1 -VerifyOnly
```

The installer downloads only missing files from Poly Haven, verifies every
checksum, builds the local Editor module if needed, and imports masked,
two-sided PBR foliage into
`/Game/SpacePatriot/OpenAssets/PolyHaven/Foliage`. It opens no gameplay
window. It refuses to replace a mismatched or partially imported local
asset. Re-running it after a completed install verifies and exits.

| Asset | Role | Unreal mesh path |
| --- | --- | --- |
| [Wild Rooibos Bush](https://polyhaven.com/a/wild_rooibos_bush) | Dry, scrubby slope cover | `/Game/SpacePatriot/OpenAssets/PolyHaven/Foliage/WildRooibosBush/wild_rooibos_bush_1k` |
| [Shrub 02](https://polyhaven.com/a/shrub_02) | Green valley and riparian cover | `/Game/SpacePatriot/OpenAssets/PolyHaven/Foliage/Shrub02/shrub_02_1k` |
| [Quiver Tree 02](https://polyhaven.com/a/quiver_tree_02) | Sparse desert landmark | `/Game/SpacePatriot/OpenAssets/PolyHaven/Foliage/QuiverTree02/quiver_tree_02_1k` |

The FBX, color, DirectX normal, roughness, and alpha or mask maps are imported
as separate local Unreal assets. The importer creates two-sided masked
materials so leaves do not appear as opaque rectangular cards. Runtime
dressing places at most eight desert plants or sixteen temperate plants per
landing patch, clearing the ship egress, outpost, and encounter sites. Each
mesh has four distance LODs. These plants remain placeholders for authored
alien flora; final art direction and distance culling still need review.

These assets are [Poly Haven CC0](https://polyhaven.com/license), usable in
commercial games. Their creators are credited on the linked asset pages and
in the manifest. The pinned files were resolved through Poly Haven's public
API: Powered by [Poly Haven](https://polyhaven.com/).
