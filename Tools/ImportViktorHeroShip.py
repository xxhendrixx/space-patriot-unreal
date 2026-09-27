"""Import the locally downloaded Viktor Hahn cruiser without committing pack binaries.

Run through Tools/InstallViktorHeroShip.ps1 with the Unreal Editor closed. This
creates only ignored local /Game/SpacePatriot/OpenAssets/ViktorShips assets;
the flight pawn loads the mesh at runtime while retaining its flight systems.
"""

import hashlib
import json
from pathlib import Path
import zipfile

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MANIFEST = json.loads((PROJECT / "Data" / "ViktorShipsDependency.json").read_text(encoding="utf-8"))
ARCHIVE = PROJECT / "SourceAssets" / "ExternalShips" / "ViktorHahn" / "spaceships2.zip"
STAGED = ARCHIVE.parent / "Staged"
DEST = "/Game/SpacePatriot/OpenAssets/ViktorShips"
REPORT = PROJECT / "Saved" / "ViktorShipImportReport.json"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
SOURCE_NAME = "cruiser03"
SCALE_CM = 130.0
LIFT_CM = 160.0


def verify_archive():
    if not ARCHIVE.is_file() or ARCHIVE.stat().st_size != MANIFEST["archive_bytes"]:
        raise RuntimeError("The local Viktor Hahn archive is missing or the wrong size")
    digest = hashlib.sha256(ARCHIVE.read_bytes()).hexdigest()
    if digest != MANIFEST["archive_sha256"]:
        raise RuntimeError("The local Viktor Hahn archive failed SHA256 verification")


def stage_sources():
    STAGED.mkdir(parents=True, exist_ok=True)
    names = [SOURCE_NAME + ".obj"] + [
        SOURCE_NAME + "_" + channel + ".png"
        for channel in ("diffuse", "normal", "specular", "emission")
    ]
    with zipfile.ZipFile(ARCHIVE) as packed:
        if not set(names).issubset(set(packed.namelist())):
            raise RuntimeError("The downloaded archive does not contain the selected cruiser and textures")
        for name in names[1:]:
            (STAGED / name).write_bytes(packed.read(name))
        lines = packed.read(names[0]).decode("utf-8").splitlines()

    transformed = ["# Viktor Hahn cruiser, reoriented for Unreal X-forward / Z-up"]
    vertices = 0
    for line in lines:
        if line.startswith("v ") or line.startswith("vn "):
            fields = line.split()
            if len(fields) < 4:
                raise RuntimeError("Malformed OBJ vertex: " + line)
            x, y, z = map(float, fields[1:4])
            if fields[0] == "v":
                # Source is X-width, Y-up, Z-forward in metres. A cyclic
                # axis permutation preserves winding, UVs, and normals.
                mapped = (z * SCALE_CM, x * SCALE_CM, y * SCALE_CM + LIFT_CM)
                vertices += 1
            else:
                mapped = (z, x, y)
            transformed.append("{} {:.8f} {:.8f} {:.8f}".format(fields[0], *mapped))
        else:
            transformed.append(line)
    if vertices < 1000:
        raise RuntimeError("Unexpectedly sparse cruiser source mesh")
    mesh_file = STAGED / "Cruiser03_UE.obj"
    mesh_file.write_text("\n".join(transformed) + "\n", encoding="utf-8")
    return mesh_file, vertices


def import_file(file_path, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(file_path))
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", False)
    if options is not None:
        task.set_editor_property("options", options)
    TOOLS.import_asset_tasks([task])
    paths = [str(item) for item in task.get_editor_property("imported_object_paths")]
    if not paths:
        raise RuntimeError("Unreal produced no asset from " + str(file_path))
    return paths


def main():
    verify_archive()
    mesh_file, source_vertices = stage_sources()
    unreal.EditorAssetLibrary.make_directory(DEST)

    textures = {}
    for channel in ("diffuse", "normal", "specular", "emission"):
        name = SOURCE_NAME + "_" + channel
        paths = import_file(STAGED / (name + ".png"))
        texture = unreal.EditorAssetLibrary.load_asset(paths[0])
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError("Expected Texture2D: " + str(paths))
        if channel == "normal":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb", False)
        elif channel in ("specular", "emission"):
            texture.set_editor_property("srgb", False)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
        textures[channel] = texture

    material = TOOLS.create_asset("M_ViktorCruiser", DEST, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Could not create cruiser material")
    samples = {}
    for index, channel in enumerate(("diffuse", "normal", "specular", "emission")):
        sample = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample,
                                                -500, index * 180 - 200)
        sample.set_editor_property("texture", textures[channel])
        if channel == "normal":
            sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        samples[channel] = sample
    MEL.connect_material_property(samples["diffuse"], "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(samples["normal"], "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(samples["specular"], "R", unreal.MaterialProperty.MP_SPECULAR)
    MEL.connect_material_property(samples["emission"], "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    metal = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -220, 540)
    metal.set_editor_property("r", 0.42)
    MEL.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    rough = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -220, 640)
    rough.set_editor_property("r", 0.69)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    static_data = options.get_editor_property("static_mesh_import_data")
    static_data.set_editor_property("combine_meshes", True)
    paths = import_file(mesh_file, options)
    meshes = [unreal.EditorAssetLibrary.load_asset(path) for path in paths]
    meshes = [mesh for mesh in meshes if isinstance(mesh, unreal.StaticMesh)]
    if len(meshes) != 1 or meshes[0].get_name() != "Cruiser03_UE":
        raise RuntimeError("Expected one combined Cruiser03_UE StaticMesh, got " + str(paths))
    mesh = meshes[0]
    mesh.set_material(0, material)
    mesh.set_editor_property("light_map_coordinate_index", 0)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    bounds = mesh.get_bounds()
    if not (bounds.box_extent.x > 900 and bounds.box_extent.y > 500
            and bounds.box_extent.z > 300 and bounds.origin.z > 50):
        raise RuntimeError("Unexpected cruiser orientation/size: " + str(bounds))
    expected_packages = [PROJECT / relative for relative in MANIFEST["hero_local_asset_files"]]
    if not all(path.is_file() for path in expected_packages):
        raise RuntimeError("Import did not create all declared local assets")
    report = {
        "source_archive_sha256": MANIFEST["archive_sha256"],
        "source_vertices": source_vertices,
        "mesh": mesh.get_path_name(),
        "material": material.get_path_name(),
        "bounds_origin_cm": [bounds.origin.x, bounds.origin.y, bounds.origin.z],
        "bounds_extent_cm": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
        "packaging": "Local ignored dependency; CC BY 3.0 attribution in Data/ViktorShipsDependency.json",
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_VIKTOR_HERO_SHIP_IMPORTED " + json.dumps(report))


main()
