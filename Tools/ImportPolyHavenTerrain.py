"""Import verified Poly Haven CC0 terrain textures; leaves material/map work separate.

Run through UnrealEditor-Cmd.exe <project.uproject>
    -ExecutePythonScript=Tools/ImportPolyHavenTerrain.py
"""

from pathlib import Path
import json
import traceback
import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE = PROJECT / "SourceAssets" / "PolyHaven"
BASE = "/Game/SpacePatriot/OpenAssets/PolyHaven/Terrain"
REPORT = PROJECT / "Data" / "PolyHavenTerrainImportReport.json"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

# Native tile widths are taken from each Poly Haven product page (centimetres).
SETS = (
    ("rocks_ground_02", "RocksGround02", 200.0, {
        "diffuse": ("rocks_ground_02_col_2k.jpg", "rocks_ground_02_diff_2k"),
        "normal": ("rocks_ground_02_nor_dx_1k.png", "rocks_ground_02_nor_dx_1k"),
        "roughness": ("rocks_ground_02_rough_1k.jpg", "rocks_ground_02_rough_1k"),
        "ao": ("rocks_ground_02_ao_1k.jpg", "rocks_ground_02_ao_1k"),
    }),
    ("grass_ground", "GrassGround", 251.0, {
        "diffuse": ("grass_ground_diff_2k.jpg", "grass_ground_diff_2k"),
        "normal": ("grass_ground_nor_dx_1k.png", "grass_ground_nor_dx_1k"),
        "roughness": ("grass_ground_rough_1k.jpg", "grass_ground_rough_1k"),
        "ao": ("grass_ground_ao_1k.jpg", "grass_ground_ao_1k"),
    }),
    ("red_sand", "RedSand", 300.0, {
        "diffuse": ("red_sand_diff_2k.jpg", "red_sand_diff_2k"),
        "normal": ("red_sand_nor_dx_1k.png", "red_sand_nor_dx_1k"),
        "roughness": ("red_sand_rough_1k.jpg", "red_sand_rough_1k"),
        "ao": ("red_sand_ao_1k.jpg", "red_sand_ao_1k"),
    }),
)


def import_texture(source_path, destination, asset_name, channel):
    if not source_path.is_file():
        raise FileNotFoundError(source_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    TOOLS.import_asset_tasks([task])
    paths = [str(p) for p in task.get_editor_property("imported_object_paths")]
    if len(paths) != 1:
        raise RuntimeError("Expected one Texture2D from " + str(source_path) + ": " + str(paths))
    texture = unreal.EditorAssetLibrary.load_asset(paths[0])
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Expected Texture2D: " + paths[0])
    if channel == "normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
    elif channel in ("roughness", "ao"):
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("srgb", False)
    else:
        texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    actual_srgb = bool(texture.get_editor_property("srgb"))
    actual_compression = str(texture.get_editor_property("compression_settings"))
    if channel == "normal" and (actual_srgb or "NORMALMAP" not in actual_compression):
        raise RuntimeError("Incorrect normal import settings: " + paths[0])
    if channel in ("roughness", "ao") and actual_srgb:
        raise RuntimeError("Incorrect non-color import settings: " + paths[0])
    return {
        "source": str(source_path.relative_to(PROJECT)).replace("\\", "/"),
        "asset": paths[0],
        "srgb": actual_srgb,
        "compression": actual_compression,
    }


def main():
    result = {
        "license": "Poly Haven CC0; https://polyhaven.com/license",
        "source_manifest": "SourceAssets/PolyHaven/manifest.json",
        "sets": {},
        "note": "Texture assets only. No materials, meshes, Blueprints, or maps were modified.",
    }
    for slug, folder, native_tile_width_cm, maps in SETS:
        destination = BASE + "/" + folder
        unreal.EditorAssetLibrary.make_directory(destination)
        record = {
            "source_page": "https://polyhaven.com/a/" + slug,
            "native_tile_width_cm": native_tile_width_cm,
            "destination": destination,
            "textures": {},
        }
        for channel, (filename, asset_name) in maps.items():
            record["textures"][channel] = import_texture(
                SOURCE / slug / filename, destination, asset_name, channel
            )
        result["sets"][slug] = record
    REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_POLYHAVEN_TERRAIN_IMPORT_OK")


try:
    main()
except Exception:
    REPORT.write_text(traceback.format_exc(), encoding="utf-8")
    raise
