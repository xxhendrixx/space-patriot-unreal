# Worldworks terrain material handoff

`/Game/SpacePatriot/Materials/M_WorldTerrainPBR` is a versioned project material. Run `Tools/InstallLocalDependencies.ps1` to restore the Poly Haven textures it references. `Tools/BuildWorldTerrainPBR.py` can rebuild the material after the terrain textures are imported; it leaves the existing `M_WorldVertex` and maps untouched. `Tools/ValidateWorldTerrainPBR.py` checks its 12 texture objects, nine `WorldAlignedTexture` calls, three `WorldAlignedNormal` calls, vertex palette input, and Base Color/Normal/Roughness/AO connections.

`ASPWorldSurface` has separate `SurfaceMaterial` and optional `DetailMaterial` properties. `Tools/AssignWorldTerrainPBR.py` now sets only `DetailMaterial` to `M_WorldTerrainPBR` on the saved Kestrel flight and Kellen Reach walk maps. `Data/WorldTerrainMapAssignment.json` confirms both maps reload with `M_WorldVertex` still on the distant globe. A null detail override preserves the earlier single-material behavior in other maps. `Tools/ValidateWorldTerrainMaterialSplit.py` tests both assignments with a temporary actor.

The material uses CC0 [rocks_ground_02](https://polyhaven.com/a/rocks_ground_02), [grass_ground](https://polyhaven.com/a/grass_ground), and [red_sand](https://polyhaven.com/a/red_sand). Each set has albedo, DirectX normal, roughness, and ambient-occlusion maps. The texture sets tile in world centimetres at their source widths (200, 251, and 300 cm). The Unreal [WorldAlignedTexture and WorldAlignedNormal](https://dev.epicgames.com/documentation/unreal-engine/texturing-material-functions-in-unreal-engine) functions use the XYZ triplanar output so globe-longitude UV seams do not drive color or normal placement. The material interprets the projected normals in world space. The source `SPWorldSurface` vertex color remains a 30% biome tint and supplies grass/arid blending weights.

For a deliberate material rebuild or validation after dependency installation:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' '<project>\SpacePatriotUnreal.uproject' '-ExecutePythonScript=<project>\Tools\BuildWorldTerrainPBR.py' -unattended -nop4 -nosplash -nullrhi
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' '<project>\SpacePatriotUnreal.uproject' '-ExecutePythonScript=<project>\Tools\ValidateWorldTerrainPBR.py' -unattended -nop4 -nosplash -nullrhi
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' '<project>\SpacePatriotUnreal.uproject' '-ExecutePythonScript=<project>\Tools\ValidateWorldTerrainMaterialSplit.py' -unattended -nop4 -nosplash -nullrhi
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' '<project>\SpacePatriotUnreal.uproject' '-ExecutePythonScript=<project>\Tools\AssignWorldTerrainPBR.py' -unattended -nop4 -nosplash -nullrhi
```

The saved-asset and assignment checks pass, but the material still needs a human PIE comparison against the source-colored Earth terrain at ground height, normal-orientation inspection on steep slopes, and shader-cost measurement. Twelve triplanar function calls can be expensive, which is why only the bounded near patch uses them. Gas worlds and icy worlds need distinct material variants; these three texture sets are not a complete visual library.
