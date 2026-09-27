"""Import the accepted symmetric Kestrel source meshes and source textures.

Run with UnrealEditor-Cmd.exe SpacePatriotUnreal.uproject -ExecutePythonScript=Tools/ImportKestrel.py
or from the Unreal Editor Python console. The generated .uasset files are committed;
this script and SourceAssets make reimport reproducible for contributors.
"""

from pathlib import Path
import json
import traceback
import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE = ROOT / "SourceAssets" / "KestrelK017"
DEST = "/Game/SpacePatriot/Ships/KestrelK017"
REPORT = ROOT / "Data" / "KestrelImportReport.json"


def import_one(filename, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(SOURCE / filename))
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    if options is not None:
        task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return [str(p) for p in task.get_editor_property("imported_object_paths")]


def main():
    unreal.EditorAssetLibrary.make_directory(DEST)
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    static_data = options.get_editor_property("static_mesh_import_data")
    static_data.set_editor_property("combine_meshes", False)
    static_data.set_editor_property("import_mesh_lods", False)
    mesh_paths = import_one("Kestrel_K017.fbx", options)
    textures = {}
    for name in (
        "Kestrel_BaseColor.png",
        "Kestrel_BaseColor_LOD1.png",
        "Kestrel_Normal.png",
        "Kestrel_MetallicSmoothness.png",
        "Kestrel_Roughness_source.png",
    ):
        textures[name] = import_one(name)
    assets = unreal.EditorAssetLibrary.list_assets(DEST, recursive=False, include_folder=False)
    report = {"source_fbx": "SourceAssets/KestrelK017/Kestrel_K017.fbx", "mesh_paths": mesh_paths, "texture_paths": textures, "assets": [str(a) for a in assets]}
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_KESTREL_IMPORT " + str(len(mesh_paths)) + " meshes")


try:
    main()
except Exception:
    REPORT.write_text(traceback.format_exc(), encoding="utf-8")
    raise
