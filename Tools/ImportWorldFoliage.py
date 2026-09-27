"""Import the locally downloaded Poly Haven foliage listed in WorldFoliageDependencies.json.

Run through UnrealEditor-Cmd.exe on /Engine/Maps/Entry. This script does not edit
maps or project gameplay content; generated packages live in the ignored
/Game/SpacePatriot/OpenAssets/PolyHaven/Foliage tree.
"""

import json
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MANIFEST = PROJECT / "Data" / "WorldFoliageDependencies.json"
SOURCE = PROJECT / "SourceAssets" / "PolyHaven"
DESTINATION = "/Game/SpacePatriot/OpenAssets/PolyHaven/Foliage"
REPORT = PROJECT / "Saved" / "WorldFoliageImportReport.json"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MATERIAL = unreal.MaterialEditingLibrary


def import_file(file_path, package_dir, options=None):
    if not file_path.is_file():
        raise FileNotFoundError(file_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(file_path))
    task.set_editor_property("destination_path", package_dir)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", False)
    if options is not None:
        task.set_editor_property("options", options)
    TOOLS.import_asset_tasks([task])
    paths = [str(path) for path in task.get_editor_property("imported_object_paths")]
    if not paths:
        raise RuntimeError("Import returned no objects for " + str(file_path))
    return paths


def load_texture(path, package_dir, channel):
    imported = import_file(path, package_dir)
    texture = unreal.EditorAssetLibrary.load_asset(imported[0])
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Expected Texture2D: " + imported[0])
    if channel == "normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
    elif channel in ("roughness", "opacity"):
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("srgb", False)
    else:
        texture.set_editor_property("srgb", True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError("Could not save Texture2D " + imported[0])
    return texture


def make_material(folder, package_dir, textures):
    name = "M_PH_" + folder
    path = package_dir + "/" + name
    material = unreal.EditorAssetLibrary.load_asset(path)
    if material is None:
        material = TOOLS.create_asset(name, package_dir, unreal.Material, unreal.MaterialFactoryNew())
    else:
        MATERIAL.delete_all_material_expressions(material)
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Could not create foliage material " + path)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)
    for channel, y, target, output in (
        ("diffuse", -450, unreal.MaterialProperty.MP_BASE_COLOR, "RGB"),
        ("normal", -150, unreal.MaterialProperty.MP_NORMAL, "RGB"),
        ("roughness", 150, unreal.MaterialProperty.MP_ROUGHNESS, "R"),
        ("opacity", 450, unreal.MaterialProperty.MP_OPACITY_MASK, "R"),
    ):
        sample = MATERIAL.create_material_expression(material, unreal.MaterialExpressionTextureSample, -500, y)
        sample.set_editor_property("texture", textures[channel])
        if channel == "normal":
            sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        MATERIAL.connect_material_property(sample, output, target)
    MATERIAL.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Could not save foliage material " + path)
    return material


def import_mesh(path, package_dir, material):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    mesh_data = options.get_editor_property("static_mesh_import_data")
    mesh_data.set_editor_property("combine_meshes", True)
    mesh_data.set_editor_property("import_mesh_lods", False)
    paths = import_file(path, package_dir, options)
    meshes = []
    lod_subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    lod_options = unreal.StaticMeshReductionOptions()
    lod_options.auto_compute_lod_screen_size = False
    lod_options.reduction_settings = [
        unreal.StaticMeshReductionSettings(percent_triangles=1.0, screen_size=1.0),
        unreal.StaticMeshReductionSettings(percent_triangles=0.50, screen_size=0.40),
        unreal.StaticMeshReductionSettings(percent_triangles=0.22, screen_size=0.17),
        unreal.StaticMeshReductionSettings(percent_triangles=0.08, screen_size=0.05),
    ]
    for imported_path in paths:
        mesh = unreal.EditorAssetLibrary.load_asset(imported_path)
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError("Expected StaticMesh: " + imported_path)
        slots = len(mesh.get_editor_property("static_materials"))
        if slots < 1:
            raise RuntimeError("Foliage mesh has no material slots: " + imported_path)
        for slot in range(slots):
            mesh.set_material(slot, material)
        original_triangles = mesh.get_num_triangles(0)
        lod_subsystem.set_lods(mesh, lod_options)
        triangles = [mesh.get_num_triangles(index) for index in range(mesh.get_num_lods())]
        if len(triangles) != 4 or triangles[0] != original_triangles:
            raise RuntimeError("Foliage LOD generation failed: {} {}".format(imported_path, triangles))
        if any(triangles[index] >= triangles[index - 1] for index in range(1, 4)):
            raise RuntimeError("Foliage LODs did not reduce: {} {}".format(imported_path, triangles))
        if not unreal.EditorAssetLibrary.save_loaded_asset(mesh):
            raise RuntimeError("Could not save foliage mesh " + imported_path)
        bounds = mesh.get_bounding_box()
        dimensions = [
            round(bounds.max.x - bounds.min.x, 2),
            round(bounds.max.y - bounds.min.y, 2),
            round(bounds.max.z - bounds.min.z, 2),
        ]
        if min(dimensions) <= 0 or max(dimensions) < 20 or max(dimensions) > 15000:
            raise RuntimeError("Implausible foliage scale: {} {}".format(imported_path, dimensions))
        meshes.append({
            "path": imported_path,
            "bounds_cm": dimensions,
            "triangles_by_lod": triangles,
            "lod_count": len(triangles),
            "material_slots": slots,
        })
    return meshes


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    results = []
    for asset in manifest["assets"]:
        slug, folder = asset["id"], asset["folder"]
        package_dir = DESTINATION + "/" + folder
        unreal.EditorAssetLibrary.make_directory(package_dir)
        files = {file["channel"]: SOURCE / slug / file["filename"] for file in asset["files"]}
        textures = {
            channel: load_texture(files[channel], package_dir, channel)
            for channel in ("diffuse", "normal", "roughness", "opacity")
        }
        material = make_material(folder, package_dir, textures)
        meshes = import_mesh(files["mesh"], package_dir, material)
        results.append({"id": slug, "source": asset["source_page"], "meshes": meshes})
        unreal.log("SPACE_PATRIOT_FOLIAGE_IMPORTED {} {}".format(slug, meshes))
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps({"assets": results}, indent=2), encoding="utf-8")


main()
