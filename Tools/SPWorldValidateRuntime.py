"""Exercise native ASPWorldSurface against every source world inside Unreal Editor.

Run after compiling C++. If L_KestrelFlight has no SPWorldSurface yet, the
script spawns a temporary actor and marks the report as not map-integrated.
With one placed actor, it validates that actor, restores its original WorldId,
and saves the map.
"""

import json
from pathlib import Path
import unreal

PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MAP = "/Game/SpacePatriot/Maps/L_KestrelFlight"
REPORT = PROJECT / "Data" / "WorldRuntimeValidation.json"


def property_of(obj, name):
    return obj.get_editor_property(name)


def main():
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Could not load the Kestrel flight map")
    actors = [a for a in unreal.EditorLevelLibrary.get_all_level_actors() if isinstance(a, unreal.SPWorldSurface)]
    if len(actors) > 1:
        raise RuntimeError(f"Expected at most one SPWorldSurface, found {len(actors)}")
    integrated = len(actors) == 1
    surface = actors[0] if integrated else unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SPWorldSurface, unreal.Vector(0.0, 0.0, 0.0)
    )
    if surface is None:
        raise RuntimeError("Unable to spawn SPWorldSurface for validation")
    original_id = property_of(surface, "world_id")
    worlds = json.loads((PROJECT / "Data" / "Worlds.json").read_text(encoding="utf-8"))["worlds"]
    checks = []
    try:
        for world in worlds:
            ident = world["id"]
            if not surface.activate_world(ident):
                raise RuntimeError("World activation failed: " + ident)
            terrain_res = property_of(surface, "source_terrain_resolution")
            climate_width = property_of(surface, "source_climate_width")
            vertices = property_of(surface, "detail_vertices")
            triangles = property_of(surface, "detail_triangles")
            assert climate_width == 256, (ident, climate_width)
            assert terrain_res == (0 if world["biome"] == "gas" else 129), (ident, terrain_res)
            assert 0 <= vertices <= 9409 and 0 <= triangles <= 18432, (ident, vertices, triangles)
            sample = surface.sample_at_world_location(unreal.Vector(100000.0, 25000.0, 0.0))
            checks.append({
                "world": ident,
                "biome": world["biome"],
                "terrain_resolution": terrain_res,
                "climate_width": climate_width,
                "detail_vertices": vertices,
                "detail_triangles": triangles,
                "sample_elevation_m": property_of(sample, "elevation_meters"),
                "sample_moisture": property_of(sample, "moisture"),
            })
        if len({round(x["sample_elevation_m"], 2) for x in checks if x["biome"] != "gas"}) < 7:
            raise RuntimeError("Solid-world terrain did not vary as expected")
        report = {"map": MAP, "integrated_map_actor": integrated, "worlds_checked": len(checks), "checks": checks,
                  "max_detail_vertices": max(x["detail_vertices"] for x in checks),
                  "max_detail_triangles": max(x["detail_triangles"] for x in checks)}
        REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
        unreal.log("SPACE_PATRIOT_WORLD_RUNTIME_PASS " + json.dumps({k: v for k, v in report.items() if k != "checks"}))
    finally:
        if integrated:
            if not surface.activate_world(original_id):
                raise RuntimeError("Failed to restore source world " + original_id)
            unreal.EditorLevelLibrary.save_current_level()
        else:
            unreal.EditorLevelLibrary.destroy_actor(surface)


main()
