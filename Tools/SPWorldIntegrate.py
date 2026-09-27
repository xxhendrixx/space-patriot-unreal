"""Idempotently place the native Worldworks renderer in the Kestrel flight map.

Run SPWorldCreateMaterial.py first. This script must run inside Unreal Editor
after the C++ module has been compiled. It creates/saves the map actor, then
reloads the map to verify the actor and material are actually persisted.
"""

import json
from pathlib import Path
import traceback
import unreal

PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MAP = "/Game/SpacePatriot/Maps/L_KestrelFlight"
MATERIAL = "/Game/SpacePatriot/Materials/M_WorldVertex"
REPORT = PROJECT / "Data" / "WorldIntegrationReport.json"


def world_actors():
    return [
        actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if isinstance(actor, unreal.SPWorldSurface)
    ]


def main():
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL)
    if material is None:
        raise RuntimeError("World vertex-color material is missing; run Tools/SPWorldCreateMaterial.py first")
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Could not load " + MAP)
    actors = world_actors()
    if len(actors) > 1:
        raise RuntimeError(f"The flight map has {len(actors)} world surfaces; expected one")
    actor = actors[0] if actors else unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SPWorldSurface, unreal.Vector(0.0, 0.0, 0.0)
    )
    if actor is None:
        raise RuntimeError("Unreal could not spawn SPWorldSurface")
    actor.set_actor_label("20-Worldworks radial world surface")
    actor.set_actor_location(unreal.Vector(0.0, 0.0, 0.0), False, False)
    actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
    actor.set_editor_property("world_id", "earth")
    actor.set_editor_property("surface_material", material)
    if not actor.rebuild_surface():
        raise RuntimeError("SPWorldSurface failed to load the Earth source fields")
    if actor.get_editor_property("source_terrain_resolution") != 129 or actor.get_editor_property("source_climate_width") != 256:
        raise RuntimeError("The placed surface did not load the original Worldworks terrain and climate")
    unreal.EditorLevelLibrary.save_current_level()

    # Fresh load verifies persistence rather than merely inspecting the actor
    # that was just spawned in the editor's transient world.
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Could not reopen saved flight map")
    persisted = world_actors()
    if len(persisted) != 1:
        raise RuntimeError(f"Saved flight map has {len(persisted)} world surfaces")
    world = persisted[0]
    if world.get_editor_property("world_id") != "earth" or world.get_editor_property("surface_material") != material:
        raise RuntimeError("Saved world ID or material does not match the authored setup")
    location = world.get_actor_location()
    if max(abs(location.x), abs(location.y), abs(location.z)) > 0.01:
        raise RuntimeError(f"The world surface must be at the local origin, got {location}")
    report = {
        "map": MAP,
        "actor_class": "ASPWorldSurface",
        "actor_count": 1,
        "world_id": "earth",
        "material": MATERIAL,
        "source_terrain_resolution": world.get_editor_property("source_terrain_resolution"),
        "source_climate_width": world.get_editor_property("source_climate_width"),
        "saved_and_reloaded": True,
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_WORLD_INTEGRATED " + json.dumps(report))


try:
    main()
except Exception:
    REPORT.write_text(traceback.format_exc(), encoding="utf-8")
    raise
