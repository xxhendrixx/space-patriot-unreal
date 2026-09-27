"""Read-only saved-map and Blueprint check for the playable travel bridge."""

import json
from pathlib import Path
import unreal

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
report = project / "Data" / "TravelPlayableValidation.json"
checks = []


def check(name, condition, detail=""):
    checks.append({"name": name, "passed": bool(condition), "detail": str(detail)})
    if not condition:
        unreal.log_error("TRAVEL_PLAYABLE_FAIL " + name + " " + str(detail))


root = "/Game/SpacePatriot"
ship = unreal.EditorAssetLibrary.load_asset(root + "/Blueprints/BP_KestrelFlyable")
mode = unreal.EditorAssetLibrary.load_asset(root + "/Blueprints/BP_KestrelGameMode")
check("flyable_kestrel_blueprint", isinstance(ship, unreal.Blueprint))
check("kestrel_game_mode", isinstance(mode, unreal.Blueprint))
if ship:
    cdo = unreal.get_default_object(ship.generated_class())
    check("inherits_native_flight_pawn", isinstance(cdo, unreal.SPFlightPawn), type(cdo))
    nav = cdo.get_component_by_class(unreal.SPTravelNavigationComponent)
    visual = cdo.get_component_by_class(unreal.SPHyperdriveVisualComponent)
    camera = cdo.get_component_by_class(unreal.CameraComponent)
    check("pawn_owns_travel_authority", nav is not None)
    check("pawn_owns_hyperdrive_visuals", visual is not None)
    check("pawn_owns_flight_camera", camera is not None)
    if visual:
        check("speed_effect_material", visual.get_editor_property("effect_material") is not None)

if mode and ship:
    pawn_class = unreal.get_default_object(mode.generated_class()).get_editor_property("default_pawn_class")
    check("game_mode_spawns_travel_pawn", pawn_class == ship.generated_class(), pawn_class)

map_path = root + "/Maps/L_KestrelFlight"
check("saved_flight_map_loads", unreal.EditorLevelLibrary.load_level(map_path))
world = unreal.EditorLevelLibrary.get_editor_world()
check("map_uses_kestrel_game_mode", world.get_world_settings().get_editor_property("default_game_mode") == mode.generated_class() if mode else False)
surfaces = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if isinstance(actor, unreal.SPWorldSurface)]
check("one_live_world_surface", len(surfaces) == 1, len(surfaces))
if len(surfaces) == 1:
    check("map_starts_at_earth", surfaces[0].get_editor_property("world_id") == "earth")
    check("source_world_surface_loaded", surfaces[0].get_editor_property("source_climate_width") == 256)

input_text = (project / "Config" / "DefaultInput.ini").read_text(encoding="utf-8")
for action, key in (("SPTravelNext", "N"), ("SPTravelJump", "J"), ("SPTravelCancel", "K")):
    check("input_" + action.lower(), f'ActionName="{action}",Key={key}' in input_text)

result = {"checks": checks, "passed": sum(c["passed"] for c in checks), "failed": sum(not c["passed"] for c in checks)}
report.write_text(json.dumps(result, indent=2), encoding="utf-8")
unreal.log("TRAVEL_PLAYABLE_VALIDATION " + json.dumps({"passed": result["passed"], "failed": result["failed"]}))
if result["failed"]:
    raise RuntimeError("Travel playable validation failed; see Data/TravelPlayableValidation.json")
