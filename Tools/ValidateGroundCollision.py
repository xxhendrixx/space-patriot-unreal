"""Check immediate walkable collision in the actual Kestrel map.

Run in UnrealEditor-Cmd after compiling the native module. This does not save
the map. It probes close terrain outside the apron, the far globe beyond the
streamed patch, and the gas-world no-ground exception.
"""

import json
import time
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Validation" / "ground-collision.json"
MAP = "/Game/SpacePatriot/Maps/L_KestrelFlight"


def trace(world, x, y, top, bottom):
    result = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(x, y, top),
        unreal.Vector(x, y, bottom),
        unreal.TraceTypeQuery.ECC_VISIBILITY,
        True,
        unreal.Array(unreal.Actor),
        unreal.DrawDebugTrace.NONE,
        True,
    )
    return result.to_dict() if result else None


def must_hit_world(world, label, x, y, top, bottom, expected_component=None):
    hit = trace(world, x, y, top, bottom)
    if hit is None or hit["hit_actor"].get_class().get_name() != "SPWorldSurface":
        raise RuntimeError(f"{label}: expected Worldworks terrain, got {hit}")
    component = hit["hit_component"].get_name()
    if expected_component and component != expected_component:
        raise RuntimeError(f"{label}: expected {expected_component} collision, got {hit}")
    location = hit["location"]
    return {"name": label, "hit_actor_class": hit["hit_actor"].get_class().get_name(),
            "hit_component": component,
            "location_cm": [round(location.x, 2), round(location.y, 2), round(location.z, 2)]}


def main():
    REPORT.unlink(missing_ok=True)
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Cannot load committed Kestrel map")
    actors = [a for a in unreal.EditorLevelLibrary.get_all_level_actors() if isinstance(a, unreal.SPWorldSurface)]
    if len(actors) != 1:
        raise RuntimeError(f"Expected one Worldworks actor; found {len(actors)}")
    surface = actors[0]
    world = unreal.EditorLevelLibrary.get_editor_world()
    original_id = surface.get_editor_property("world_id")
    original_focus = surface.get_editor_property("focus_actor")
    report = {"map": MAP, "world_surface_count": len(actors), "probes": []}
    high_focus = None
    try:
        surface.set_editor_property("focus_actor", None)
        t0 = time.perf_counter()
        if not surface.activate_world("earth"):
            raise RuntimeError("Earth world field failed to activate")
        report["earth_rebuild_ms"] = round((time.perf_counter() - t0) * 1000, 2)
        report["detail_vertices"] = surface.get_editor_property("detail_vertices")
        report["focus_altitude_m"] = surface.get_editor_property("last_focus_altitude_meters")
        report["detail_collision"] = str(surface.get_editor_property("detail_mesh").get_collision_enabled())
        report["probes"].append(must_hit_world(world, "near-detail", 10000, 0, 2000, -3000,
                                              "Streamed Worldworks Detail"))
        high_focus = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.Actor, unreal.Vector(0, 0, 200000))
        if high_focus is None:
            raise RuntimeError("Could not spawn temporary high-altitude focus")
        surface.set_editor_property("focus_actor", high_focus)
        if not surface.rebuild_surface():
            raise RuntimeError("High-altitude Earth field failed to rebuild")
        report["high_focus_altitude_m"] = surface.get_editor_property("last_focus_altitude_meters")
        if report["high_focus_altitude_m"] <= 1200:
            raise RuntimeError("High-altitude probe did not hide local detail")
        report["probes"].append(must_hit_world(world, "far-globe", 500000, 0, 50000, -200000,
                                              "Source Planet"))
        if not surface.activate_world("jupiter"):
            raise RuntimeError("Jupiter world field failed to activate")
        gas_hit = trace(world, 500000, 0, 50000, -200000)
        if gas_hit is not None and gas_hit["hit_actor"].get_class().get_name() == "SPWorldSurface":
            raise RuntimeError("Gas world incorrectly has a solid planet collider")
        report["gas_world_solid_hit"] = False
        surface.set_editor_property("focus_actor", None)
        if not surface.activate_world("earth"):
            raise RuntimeError("Earth world failed to restore")
        report["probes"].append(must_hit_world(world, "near-after-world-switch", 10000, 0, 2000, -3000,
                                              "Streamed Worldworks Detail"))
        report["passed"] = True
        REPORT.parent.mkdir(parents=True, exist_ok=True)
        REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
        unreal.log("SPACE_PATRIOT_GROUND_COLLISION_PASS " + json.dumps(report))
    finally:
        surface.set_editor_property("focus_actor", original_focus)
        if surface.get_editor_property("world_id") != original_id or high_focus is not None:
            surface.activate_world(original_id)
        if high_focus is not None:
            unreal.EditorLevelLibrary.destroy_actor(high_focus)


main()
