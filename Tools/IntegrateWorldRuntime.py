"""Place the compiled world simulation Blueprint into the flight map.

Requires a successful SpacePatriotUnrealEditor C++ build and
CreateBlueprintHandoff.py to have created BP_WorldRuntime.
"""

import json
from pathlib import Path
import traceback
import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Data" / "NativeWorldRuntimeReport.json"
MAP = "/Game/SpacePatriot/Maps/L_KestrelFlight"
BP = "/Game/SpacePatriot/Blueprints/BP_WorldRuntime"


def main():
    blueprint = unreal.EditorAssetLibrary.load_asset(BP)
    if blueprint is None:
        raise RuntimeError("Compiled BP_WorldRuntime is missing")
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Could not load Kestrel flight map")
    matching = [a for a in unreal.EditorLevelLibrary.get_all_level_actors() if isinstance(a, unreal.SPWorldRuntime)]
    if len(matching) > 1:
        raise RuntimeError("Map must have a single authoritative world runtime")
    actor = matching[0] if matching else unreal.EditorLevelLibrary.spawn_actor_from_class(blueprint.generated_class(), unreal.Vector(0, 0, 0))
    actor.set_actor_label("19-world society and wildlife simulation")
    systems = actor.get_editor_property("systems")
    if not systems.load_source_catalogs():
        raise RuntimeError("C++ world/creature JSON catalogs did not load")
    if not systems.set_active_world("earth"):
        raise RuntimeError("Earth is missing from the world catalog")
    worlds = systems.get_editor_property("worlds")
    creatures = systems.get_editor_property("creature_catalog")
    earth = systems.get_wildlife_for_world("earth")
    if len(worlds) != 19 or len(creatures) != 190 or len(earth) != 10:
        raise RuntimeError(f"Catalog parity failed: {len(worlds)} worlds, {len(creatures)} creatures, {len(earth)} Earth creatures")
    unreal.EditorLevelLibrary.save_current_level()
    report = {"map": MAP, "world_runtime": BP, "worlds_loaded": len(worlds), "creatures_loaded": len(creatures), "earth_creatures": len(earth), "world_runtime_actors": 1}
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_NATIVE_WORLD_RUNTIME " + json.dumps(report))


try:
    main()
except Exception:
    REPORT.write_text(traceback.format_exc(), encoding="utf-8")
    raise
