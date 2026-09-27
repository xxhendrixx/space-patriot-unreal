"""Import the verified Poly Haven CC0 environment starter set into UE 5.8.

Copy the staged files to SourceAssets/PolyHaven, then run this through
UnrealEditor-Cmd.exe <project.uproject> -ExecutePythonScript=Tools/ImportPolyHaven.py.
The import only creates assets; it does not edit or save a level.
"""

from pathlib import Path
import json
import traceback
import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE = PROJECT / "SourceAssets" / "PolyHaven"
BASE = "/Game/SpacePatriot/OpenAssets/PolyHaven"
CONCRETE = BASE + "/HangarConcrete"
BOULDER = BASE + "/Boulder01"
CRATE = BASE + "/PlasticCrate02"
REPORT = PROJECT / "Data" / "PolyHavenImportReport.json"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary


def import_one(file_path, destination, options=None):
    if not file_path.is_file():
        raise FileNotFoundError(file_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(file_path))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    if options is not None:
        task.set_editor_property("options", options)
    TOOLS.import_asset_tasks([task])
    paths = [str(p) for p in task.get_editor_property("imported_object_paths")]
    if not paths:
        raise RuntimeError("Unreal import produced no asset: " + str(file_path))
    return paths


def import_texture(subdir, name, destination, channel):
    paths = import_one(SOURCE / subdir / name, destination)
    texture = unreal.EditorAssetLibrary.load_asset(paths[0])
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Expected Texture2D for " + name + ": " + str(paths))
    if channel == "normal":
        texture.set_editor_property(
            "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP
        )
        texture.set_editor_property("srgb", False)
    elif channel in ("roughness", "ao", "opacity"):
        texture.set_editor_property(
            "compression_settings", unreal.TextureCompressionSettings.TC_MASKS
        )
        texture.set_editor_property("srgb", False)
    else:
        texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture, paths


def texture_sample(material, texture, x, y, uv_node, normal=False):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample, x, y)
    node.set_editor_property("texture", texture)
    if normal:
        node.set_editor_property(
            "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        )
    if uv_node is not None:
        MEL.connect_material_expressions(uv_node, "", node, "Coordinates")
    return node


def create_pbr_material(name, destination, textures, world_tile_cm=None):
    path = destination + "/" + name
    material = unreal.EditorAssetLibrary.load_asset(path)
    if material is None:
        material = TOOLS.create_asset(name, destination, unreal.Material, unreal.MaterialFactoryNew())
        if material is None:
            raise RuntimeError("Could not create material: " + path)
    else:
        MEL.delete_all_material_expressions(material)

    uv = None
    if world_tile_cm is not None:
        # The scanned concrete is 2 m wide. On horizontal ground/apron
        # surfaces, world XY gives a real 2 m tile regardless of mesh UVs.
        world = MEL.create_material_expression(
            material, unreal.MaterialExpressionWorldPosition, -1100, 100
        )
        xy = MEL.create_material_expression(
            material, unreal.MaterialExpressionComponentMask, -900, 100
        )
        xy.set_editor_property("r", True)
        xy.set_editor_property("g", True)
        xy.set_editor_property("b", False)
        xy.set_editor_property("a", False)
        uv = MEL.create_material_expression(
            material, unreal.MaterialExpressionMultiply, -700, 100
        )
        inverse_tile = MEL.create_material_expression(
            material, unreal.MaterialExpressionConstant, -900, 300
        )
        inverse_tile.set_editor_property("r", 1.0 / float(world_tile_cm))
        MEL.connect_material_expressions(world, "", xy, "Input")
        MEL.connect_material_expressions(xy, "", uv, "A")
        MEL.connect_material_expressions(inverse_tile, "", uv, "B")

    base = texture_sample(material, textures["diffuse"], -600, -350, uv)
    normal = texture_sample(material, textures["normal"], -600, -50, uv, normal=True)
    rough = texture_sample(material, textures["roughness"], -600, 250, uv)
    MEL.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    if "ao" in textures:
        ao = texture_sample(material, textures["ao"], -600, 550, uv)
        MEL.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    if "opacity" in textures:
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        opacity = texture_sample(material, textures["opacity"], -600, 850, uv)
        MEL.connect_material_property(
            opacity, "R", unreal.MaterialProperty.MP_OPACITY_MASK
        )

    MEL.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return path


def assign_material(mesh_paths, material_path):
    material = unreal.EditorAssetLibrary.load_asset(material_path)
    if not isinstance(material, unreal.MaterialInterface):
        raise RuntimeError("Missing material: " + material_path)
    for mesh_path in mesh_paths:
        mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError("Expected StaticMesh: " + mesh_path)
        mesh.set_material(0, material)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)


def main():
    unreal.EditorAssetLibrary.make_directory(CONCRETE)
    unreal.EditorAssetLibrary.make_directory(BOULDER)
    unreal.EditorAssetLibrary.make_directory(CRATE)
    imported = {}

    concrete_files = {
        "diffuse": "hangar_concrete_floor_diff_2k.jpg",
        "normal": "hangar_concrete_floor_nor_dx_2k.png",
        "roughness": "hangar_concrete_floor_rough_2k.jpg",
        "ao": "hangar_concrete_floor_ao_2k.jpg",
    }
    concrete_textures = {}
    for channel, filename in concrete_files.items():
        concrete_textures[channel], imported["concrete_" + channel] = import_texture(
            "hangar_concrete_floor", filename, CONCRETE, channel
        )
    concrete_material = create_pbr_material(
        "M_PH_HangarConcreteXY200cm", CONCRETE, concrete_textures, world_tile_cm=200
    )

    rock_files = {
        "diffuse": "boulder_01_diff_1k.jpg",
        "normal": "boulder_01_nor_dx_1k.png",
        "roughness": "boulder_01_rough_1k.jpg",
    }
    rock_textures = {}
    for channel, filename in rock_files.items():
        rock_textures[channel], imported["boulder_" + channel] = import_texture(
            "boulder_01", filename, BOULDER, channel
        )
    rock_material = create_pbr_material(
        "M_PH_Boulder01", BOULDER, rock_textures
    )

    fbx = unreal.FbxImportUI()
    fbx.set_editor_property("import_mesh", True)
    fbx.set_editor_property("import_as_skeletal", False)
    fbx.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    fbx.set_editor_property("import_materials", False)
    fbx.set_editor_property("import_textures", False)
    mesh_data = fbx.get_editor_property("static_mesh_import_data")
    mesh_data.set_editor_property("combine_meshes", False)
    mesh_data.set_editor_property("import_mesh_lods", True)
    imported["boulder_meshes"] = import_one(
        SOURCE / "boulder_01" / "boulder_01_1k.fbx", BOULDER, fbx
    )
    assign_material(imported["boulder_meshes"], rock_material)

    crate_files = {
        "diffuse": "plastic_crate_02_diff_1k.jpg",
        "normal": "plastic_crate_02_nor_dx_1k.png",
        "roughness": "plastic_crate_02_rough_1k.jpg",
        "opacity": "plastic_crate_02_opacity_1k.png",
    }
    crate_textures = {}
    for channel, filename in crate_files.items():
        crate_textures[channel], imported["crate_" + channel] = import_texture(
            "plastic_crate_02", filename, CRATE, channel
        )
    crate_material = create_pbr_material("M_PH_PlasticCrate02", CRATE, crate_textures)
    mesh_data.set_editor_property("import_mesh_lods", False)
    imported["crate_meshes"] = import_one(
        SOURCE / "plastic_crate_02" / "plastic_crate_02_1k.fbx", CRATE, fbx
    )
    assign_material(imported["crate_meshes"], crate_material)

    report = {
        "source_manifest": "SourceAssets/PolyHaven/manifest.json",
        "license": "CC0; https://polyhaven.com/license",
        "concrete_material": concrete_material,
        "boulder_material": rock_material,
        "crate_material": crate_material,
        "imported": imported,
        "note": "No level was modified. Imported static meshes use their PBR materials by default.",
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_POLYHAVEN_IMPORT_OK " + str(len(imported["boulder_meshes"]) + len(imported["crate_meshes"])))


try:
    main()
except Exception:
    REPORT.write_text(traceback.format_exc(), encoding="utf-8")
    raise
