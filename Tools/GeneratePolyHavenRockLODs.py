"""Generate distance LODs for three CC0 shoreline rock meshes in UE 5.8.

Run with UnrealEditor-Cmd.exe <project.uproject> -run=pythonscript
    -script=<project>/Tools/GeneratePolyHavenRockLODs.py -unattended -nullrhi

Only the listed StaticMesh assets are saved. No maps are loaded or changed.
"""

import json
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Data" / "PolyHavenRockLODReport.json"
MESHES = (
    "/Game/SpacePatriot/OpenAssets/PolyHaven/Boulder01/boulder_01_LOD0",
    "/Game/SpacePatriot/OpenAssets/PolyHaven/NamaqualandBoulder03/namaqualand_boulder_03_1k",
    "/Game/SpacePatriot/OpenAssets/PolyHaven/NamaqualandBoulder05/namaqualand_boulder_05_1k",
)


def xyz(vector):
    return [round(vector.x, 3), round(vector.y, 3), round(vector.z, 3)]


subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
options = unreal.StaticMeshReductionOptions()
options.auto_compute_lod_screen_size = False
options.reduction_settings = [
    unreal.StaticMeshReductionSettings(percent_triangles=1.0, screen_size=1.0),
    unreal.StaticMeshReductionSettings(percent_triangles=0.48, screen_size=0.48),
    unreal.StaticMeshReductionSettings(percent_triangles=0.20, screen_size=0.20),
    unreal.StaticMeshReductionSettings(percent_triangles=0.07, screen_size=0.07),
]

report = {"engine": unreal.SystemLibrary.get_engine_version(), "meshes": []}
for path in MESHES:
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Missing StaticMesh: " + path)
    original_triangles = mesh.get_num_triangles(0)
    original_extent = xyz(mesh.get_bounds().box_extent)
    created = subsystem.set_lods(mesh, options)
    lod_count = mesh.get_num_lods()
    triangles = [mesh.get_num_triangles(index) for index in range(lod_count)]
    extent = xyz(mesh.get_bounds().box_extent)
    if lod_count != 4 or triangles[0] != original_triangles:
        raise RuntimeError("LOD generation failed: {} -> {}".format(path, triangles))
    if any(triangles[index] >= triangles[index - 1] for index in range(1, 4)):
        raise RuntimeError("LODs did not reduce monotonically: {} -> {}".format(path, triangles))
    if any(abs(a - b) > 1.0 for a, b in zip(original_extent, extent)):
        raise RuntimeError("LOD generation changed bounds: {} -> {}".format(path, extent))
    if not unreal.EditorAssetLibrary.save_loaded_asset(mesh):
        raise RuntimeError("Could not save " + path)
    report["meshes"].append(
        {
            "asset": path,
            "set_lods_result": created,
            "triangles": triangles,
            "box_extent_cm": extent,
        }
    )
    unreal.log("SPACE_PATRIOT_ROCK_LODS {} {}".format(path, triangles))

REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
