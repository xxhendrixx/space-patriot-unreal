"""Import two Poly Haven CC0 boulders as ready-to-place PBR StaticMeshes.

Run through UnrealEditor-Cmd.exe <project.uproject>
    -ExecutePythonScript=Tools/ImportPolyHavenRocks.py
No maps or existing Blueprints are modified.
The selected 1K FBX files import as LOD0; add reduced LODs separately.
"""

from pathlib import Path
import json
import traceback
import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE = PROJECT / "SourceAssets" / "PolyHaven"
BASE = "/Game/SpacePatriot/OpenAssets/PolyHaven"
REPORT = PROJECT / "Data" / "PolyHavenRocksImportReport.json"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
ROCKS = (
    ("namaqualand_boulder_03", "NamaqualandBoulder03", 307.0),
    ("namaqualand_boulder_05", "NamaqualandBoulder05", 136.0),
)


def import_one(filename, destination, options=None):
    if not filename.is_file():
        raise FileNotFoundError(filename)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    if options is not None:
        task.set_editor_property("options", options)
    TOOLS.import_asset_tasks([task])
    paths = [str(p) for p in task.get_editor_property("imported_object_paths")]
    if not paths:
        raise RuntimeError("No imported assets: " + str(filename))
    return paths


def import_texture(filename, destination, channel):
    path = import_one(filename, destination)[0]
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Not a Texture2D: " + path)
    if channel == "normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
    elif channel == "roughness":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("srgb", False)
    else:
        texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return path, texture


def material_for(slug, folder, textures):
    name = "M_PH_" + folder
    destination = BASE + "/" + folder
    path = destination + "/" + name
    material = unreal.EditorAssetLibrary.load_asset(path)
    if material is None:
        material = TOOLS.create_asset(name, destination, unreal.Material, unreal.MaterialFactoryNew())
    else:
        MEL.delete_all_material_expressions(material)
    if material is None:
        raise RuntimeError("Could not create material " + path)
    for channel, y, prop, output in (
        ("diffuse", -250, unreal.MaterialProperty.MP_BASE_COLOR, "RGB"),
        ("normal", 0, unreal.MaterialProperty.MP_NORMAL, "RGB"),
        ("roughness", 250, unreal.MaterialProperty.MP_ROUGHNESS, "R"),
    ):
        sample = MEL.create_material_expression(
            material, unreal.MaterialExpressionTextureSample, -500, y
        )
        sample.set_editor_property("texture", textures[channel])
        if channel == "normal":
            sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        MEL.connect_material_property(sample, output, prop)
    MEL.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return path, material


def mesh_info(path, material, expected_max_cm):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Not a StaticMesh: " + path)
    mesh.set_material(0, material)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    box = mesh.get_bounding_box()
    size_cm = [
        float(box.max.x - box.min.x),
        float(box.max.y - box.min.y),
        float(box.max.z - box.min.z),
    ]
    largest = max(size_cm)
    if not (0.75 * expected_max_cm <= largest <= 1.25 * expected_max_cm):
        raise RuntimeError("Unexpected FBX scale for " + path + ": " + str(size_cm))
    return {
        "asset": path,
        "size_cm": size_cm,
        "triangles_lod0": mesh.get_num_triangles(0),
        "num_lods": mesh.get_num_lods(),
        "material_slot_0": mesh.get_material(0).get_path_name(),
    }


def main():
    report = {
        "license": "Poly Haven CC0; https://polyhaven.com/license",
        "source_manifest": "SourceAssets/PolyHaven/manifest.json",
        "rocks": {},
        "note": "No map or Blueprint asset was modified.",
    }
    for slug, folder, expected_max_cm in ROCKS:
        destination = BASE + "/" + folder
        unreal.EditorAssetLibrary.make_directory(destination)
        source_dir = SOURCE / slug
        textures = {}
        imported = {}
        for channel, suffix in (
            ("diffuse", "diff_1k.jpg"),
            ("normal", "nor_dx_1k.png"),
            ("roughness", "rough_1k.jpg"),
        ):
            path, texture = import_texture(source_dir / (slug + "_" + suffix), destination, channel)
            imported[channel] = {
                "asset": path,
                "srgb": bool(texture.get_editor_property("srgb")),
                "compression": str(texture.get_editor_property("compression_settings")),
            }
            textures[channel] = texture
        material_path, material = material_for(slug, folder, textures)
        fbx = unreal.FbxImportUI()
        fbx.set_editor_property("import_mesh", True)
        fbx.set_editor_property("import_as_skeletal", False)
        fbx.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        fbx.set_editor_property("import_materials", False)
        fbx.set_editor_property("import_textures", False)
        mesh_data = fbx.get_editor_property("static_mesh_import_data")
        mesh_data.set_editor_property("combine_meshes", False)
        mesh_data.set_editor_property("import_mesh_lods", True)
        mesh_paths = import_one(source_dir / (slug + "_1k.fbx"), destination, fbx)
        meshes = [mesh_info(path, material, expected_max_cm) for path in mesh_paths]
        if not meshes:
            raise RuntimeError("No meshes for " + slug)
        report["rocks"][slug] = {
            "source_page": "https://polyhaven.com/a/" + slug,
            "folder": destination,
            "material": material_path,
            "textures": imported,
            "meshes": meshes,
        }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_POLYHAVEN_ROCKS_IMPORT_OK")


try:
    main()
except Exception:
    REPORT.write_text(traceback.format_exc(), encoding="utf-8")
    raise
