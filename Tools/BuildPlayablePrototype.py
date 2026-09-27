"""Build the first openable Unreal flight/map slice from imported Kestrel assets.

Run ImportKestrel.py first. This script creates committed Blueprint/material/map
assets using Unreal Editor's own APIs; no binary .uasset files are hand-written.
"""

import json
from pathlib import Path
import traceback
import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ROOT = "/Game/SpacePatriot"
SHIPS = ROOT + "/Ships/KestrelK017"
BLUEPRINTS = ROOT + "/Blueprints"
MATERIALS = ROOT + "/Materials"
MAP = ROOT + "/Maps/L_KestrelFlight"
REPORT = PROJECT / "Data" / "PlayablePrototypeReport.json"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def asset(path):
    result = unreal.EditorAssetLibrary.load_asset(path)
    if result is None:
        raise RuntimeError("Missing asset: " + path)
    return result


def make_material(name, base_texture_path, metal=0.25, rough=0.62):
    path = MATERIALS + "/" + name
    existing = unreal.EditorAssetLibrary.load_asset(path)
    if existing:
        return existing
    material = TOOLS.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError("Could not create material " + name)
    sample = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureSample, -500, -50)
    sample.set_editor_property("texture", asset(base_texture_path))
    unreal.MaterialEditingLibrary.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    metallic = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -250, 160)
    metallic.set_editor_property("r", metal)
    unreal.MaterialEditingLibrary.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -250, 260)
    roughness.set_editor_property("r", rough)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def make_solid_material(name, rgb, metal=0.0, rough=0.8):
    path = MATERIALS + "/" + name
    existing = unreal.EditorAssetLibrary.load_asset(path)
    if existing:
        return existing
    material = TOOLS.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    color.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    metallic = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 180)
    metallic.set_editor_property("r", metal)
    unreal.MaterialEditingLibrary.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 260)
    roughness.set_editor_property("r", rough)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def make_blueprint(name, parent):
    path = BLUEPRINTS + "/" + name
    existing = unreal.EditorAssetLibrary.load_asset(path)
    if existing:
        return existing
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    bp = TOOLS.create_asset(name, BLUEPRINTS, unreal.Blueprint, factory)
    if bp is None:
        raise RuntimeError("Could not create Blueprint " + name)
    return bp


def add_ship_components(bp, meshes):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    root_handle = next(h for h in handles if isinstance(lib.get_object_for_blueprint(lib.get_data(h), bp), unreal.SphereComponent))
    for h in handles:
        obj = lib.get_object_for_blueprint(lib.get_data(h), bp)
        if isinstance(obj, unreal.StaticMeshComponent) and "MeshComponent0" in obj.get_name():
            obj.set_editor_property("hidden_in_game", True)
            obj.set_editor_property("visible", False)
    existing_meshes = {
        obj.get_editor_property("static_mesh").get_name()
        for h in handles
        if isinstance((obj := lib.get_object_for_blueprint(lib.get_data(h), bp)), unreal.StaticMeshComponent)
        and obj.get_editor_property("static_mesh")
    }
    count = 0
    for part, mesh in meshes.items():
        # Re-running the bootstrap should not duplicate components.
        if part + "_LOD0" in existing_meshes:
            continue
        params = unreal.AddNewSubobjectParams()
        params.set_editor_property("parent_handle", root_handle)
        params.set_editor_property("new_class", unreal.StaticMeshComponent.static_class())
        params.set_editor_property("blueprint_context", bp)
        handle, reason = subsystem.add_new_subobject(params)
        comp = lib.get_object_for_blueprint(lib.get_data(handle), bp)
        if comp is None:
            raise RuntimeError("Could not add " + part + ": " + str(reason))
        comp.set_editor_property("static_mesh", mesh)
        comp.set_editor_property("relative_location", unreal.Vector(2200.0, 0.0, -250.0))
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        count += 1
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return count


def spawn_mesh(name, mesh, position, scale, material=None, rotation=None):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position), rotation or unreal.Rotator())
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(mesh)
    if material is not None:
        actor.static_mesh_component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def main():
    for directory in (BLUEPRINTS, MATERIALS, ROOT + "/Maps"):
        unreal.EditorAssetLibrary.make_directory(directory)
    hull_material = make_material("M_KestrelHull", SHIPS + "/Kestrel_BaseColor")
    modules_material = make_material("M_KestrelModules", SHIPS + "/Kestrel_BaseColor")
    pad_material = make_solid_material("M_DockConcrete", (0.17, 0.19, 0.18), 0.08)
    stripe_material = make_solid_material("M_DockWarning", (0.62, 0.39, 0.08), 0.08)
    water_material = make_solid_material("M_HarborWater", (0.03, 0.12, 0.16), 0.12, 0.24)
    meshes = {}
    mesh_report = {}
    for part in ("Hull", "Wing_Port", "Wing_Starboard", "Drive_Port", "Drive_Starboard", "Gear_Nose", "Gear_Port", "Gear_Starboard"):
        mesh = asset(SHIPS + "/" + part + "_LOD0")
        mesh.set_material(0, hull_material if part == "Hull" else modules_material)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        bounds = mesh.get_bounds()
        mesh_report[part] = {"origin_cm": [bounds.origin.x, bounds.origin.y, bounds.origin.z], "extent_cm": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]}
        meshes[part] = mesh
    ship_bp = make_blueprint("BP_KestrelFlyable", unreal.DefaultPawn.static_class())
    components_added = add_ship_components(ship_bp, meshes)
    mode_bp = make_blueprint("BP_KestrelGameMode", unreal.GameModeBase.static_class())
    mode_cdo = unreal.get_default_object(mode_bp.generated_class())
    mode_cdo.set_editor_property("default_pawn_class", ship_bp.generated_class())
    unreal.EditorAssetLibrary.save_loaded_asset(mode_bp)

    cube = asset("/Engine/BasicShapes/Cube")
    cylinder = asset("/Engine/BasicShapes/Cylinder")
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        unreal.EditorLevelLibrary.load_level(MAP)
    else:
        if not unreal.EditorLevelLibrary.new_level_from_template(MAP, "/Engine/Maps/Templates/Template_Default"):
            raise RuntimeError("Could not create map")
        world = unreal.EditorLevelLibrary.get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", mode_bp.generated_class())
        spawn_mesh("Kellen Reach flight apron", cube, (0, 0, -130), (2.4, 2.4, 0.16), pad_material)
        spawn_mesh("Harbor water north", cube, (0, 26000, -480), (600, 320, 2), water_material)
        for y in (-7000, 7000):
            spawn_mesh("Runway edge", cube, (0, y, -30), (170, 1.8, 0.18), stripe_material)
        for x in (-7000, 7000):
            spawn_mesh("Runway seam", cube, (x, 0, -30), (1.8, 140, 0.18), stripe_material)
        for y in (-8000, 8000):
            spawn_mesh("Landing mast base", cylinder, (8500, y, 900), (4, 4, 20), pad_material)
        # This grounded display ship makes the map inspectable before pressing Play.
        for part, mesh in meshes.items():
            spawn_mesh("Kestrel K-017 / " + part, mesh, (0, 0, 0), (1, 1, 1))
        start = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-3200, 0, 1200))
        start.set_actor_label("Kestrel flight start")
        unreal.EditorLevelLibrary.save_current_level()
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", mode_bp.generated_class())
    unreal.EditorLevelLibrary.save_current_level()
    report = {"map": MAP, "ship_blueprint": BLUEPRINTS + "/BP_KestrelFlyable", "game_mode": BLUEPRINTS + "/BP_KestrelGameMode", "new_components": components_added, "mesh_bounds": mesh_report, "map_actor_count": len(unreal.EditorLevelLibrary.get_all_level_actors())}
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_PLAYABLE_BUILT " + json.dumps(report))


try:
    main()
except Exception:
    REPORT.write_text(traceback.format_exc(), encoding="utf-8")
    raise
