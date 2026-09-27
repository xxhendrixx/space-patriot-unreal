"""Fail on missing gameplay/map assets or a Kestrel axis/mirror regression."""

import json
from pathlib import Path
import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
ROOT = "/Game/SpacePatriot"
REPORT = PROJECT / "Data" / "PlayableValidation.json"
SHIP_PARTS = ("Hull", "Wing_Port", "Wing_Starboard", "Drive_Port", "Drive_Starboard", "Gear_Nose", "Gear_Port", "Gear_Starboard")
checks = []


def check(name, condition, detail=""):
    checks.append({"name": name, "passed": bool(condition), "detail": str(detail)})
    if not condition:
        unreal.log_error("SPACE_PATRIOT_VALIDATION_FAIL " + name + " " + str(detail))


def load(path):
    return unreal.EditorAssetLibrary.load_asset(path)


meshes = {part: load(ROOT + "/Ships/KestrelK017/" + part + "_LOD0") for part in SHIP_PARTS}
check("eight_kestrel_parts", all(meshes.values()), str([p for p, m in meshes.items() if not m]))
if all(meshes.values()):
    hull = meshes["Hull"].get_bounds().origin
    check("hull_unreal_z_up", hull.z > 250 and abs(hull.y) < 1 and hull.x < 0, str(hull))
    for pair in (("Wing_Port", "Wing_Starboard"), ("Drive_Port", "Drive_Starboard")):
        left = meshes[pair[0]].get_bounds().origin
        right = meshes[pair[1]].get_bounds().origin
        passed = abs(left.x - right.x) < 0.1 and abs(left.z - right.z) < 0.1 and abs(left.y + right.y) < 0.1
        check("mirror_" + pair[0].lower(), passed, str((left, right)))
    for part, mesh in meshes.items():
        check("material_" + part.lower(), mesh.get_material(0) is not None)

ship_bp = load(ROOT + "/Blueprints/BP_KestrelFlyable")
mode_bp = load(ROOT + "/Blueprints/BP_KestrelGameMode")
check("ship_blueprint", ship_bp is not None)
check("game_mode_blueprint", mode_bp is not None)
if ship_bp:
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(ship_bp)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    components = []
    for handle in handles:
        obj = lib.get_object_for_blueprint(lib.get_data(handle), ship_bp)
        if isinstance(obj, unreal.StaticMeshComponent) and obj.get_editor_property("static_mesh"):
            components.append(str(obj.get_editor_property("static_mesh").get_name()))
    for part in SHIP_PARTS:
        check("component_" + part.lower(), part + "_LOD0" in components, str(components))
if mode_bp and ship_bp:
    pawn_class = unreal.get_default_object(mode_bp.generated_class()).get_editor_property("default_pawn_class")
    check("mode_spawns_kestrel", pawn_class == ship_bp.generated_class(), str(pawn_class))

map_path = ROOT + "/Maps/L_KestrelFlight"
check("map_exists", unreal.EditorAssetLibrary.does_asset_exist(map_path))
if unreal.EditorAssetLibrary.does_asset_exist(map_path):
    check("map_loads", unreal.EditorLevelLibrary.load_level(map_path))
    world = unreal.EditorLevelLibrary.get_editor_world()
    mode = world.get_world_settings().get_editor_property("default_game_mode")
    check("map_uses_kestrel_mode", mode == mode_bp.generated_class() if mode_bp else False, str(mode))
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    check("map_has_launch_apron_and_ship", len(actors) >= 20, len(actors))
    check("map_has_player_start", any(isinstance(actor, unreal.PlayerStart) for actor in actors))

worlds = json.loads((PROJECT / "Data" / "Worlds.json").read_text(encoding="utf-8"))["worlds"]
creatures = json.loads((PROJECT / "Data" / "CreatureRosters.json").read_text(encoding="utf-8"))["worlds"]
check("nineteen_worlds", len(worlds) == 19, len(worlds))
check("nineteen_rosters", len(creatures) == 19, len(creatures))
check("ten_creatures_per_world", all(len(w["fauna"]) == 10 for w in creatures))

summary = {"checks": checks, "passed": sum(c["passed"] for c in checks), "failed": sum(not c["passed"] for c in checks)}
REPORT.write_text(json.dumps(summary, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_VALIDATION " + str(summary["passed"]) + " passed, " + str(summary["failed"]) + " failed")
if summary["failed"]:
    raise RuntimeError("Playable Unreal prototype validation failed; see Data/PlayableValidation.json")
