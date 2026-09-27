"""Save a restrained CC0 greenbelt on the outer Kellen Reach port margins.

Run with /Game/SpacePatriot/Maps/L_KellenReachWalk loaded in UnrealEditor-Cmd,
after Tools/InstallWorldFoliage.ps1. Never run while Play in Editor is active.
The script does not touch the flyable ship, gameplay actors, floor materials,
or the source Worldworks surface. Stable labels make repeated runs idempotent.
"""

import json
import sys
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(PROJECT / "Tools"))
from KellenReachGreenbeltPlan import PREFIX, RESERVED, make_plan  # noqa: E402


MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
REPORT = PROJECT / "Data" / "KellenReachGreenbeltReport.json"
MESH_PATHS = {
    "shrub": "/Game/SpacePatriot/OpenAssets/PolyHaven/Foliage/Shrub02/shrub_02_1k",
    "rooibos": "/Game/SpacePatriot/OpenAssets/PolyHaven/Foliage/WildRooibosBush/wild_rooibos_bush_1k",
}


def loaded_actors():
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    named = {actor.get_actor_label(): actor for actor in actors}
    return actors, named


def grounded_z(mesh, scale, floor_top):
    bounds = mesh.get_bounds()
    return floor_top - (bounds.origin.z - bounds.box_extent.z) * scale


def tag_earth_greenbelt(actor):
    tags = list(actor.get_editor_property("tags"))
    existing = {str(tag) for tag in tags}
    for tag in ("SP_EarthOnly", "SP_Greenbelt"):
        if tag not in existing:
            tags.append(unreal.Name(tag))
    actor.set_editor_property("tags", tags)


def place_one(named, item, mesh, floor_top):
    label = item["label"]
    actor = named.get(label)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(item["x"], item["y"], floor_top), unreal.Rotator()
        )
        if actor is None:
            raise RuntimeError("Could not spawn " + label)
        actor.set_actor_label(label)
        named[label] = actor
    elif not isinstance(actor, unreal.StaticMeshActor):
        raise RuntimeError("Greenbelt label belongs to a non-mesh actor: " + label)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    scale = item["scale"]
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    actor.set_actor_rotation(unreal.Rotator(0, item["yaw"], 0), False)
    actor.set_actor_location(unreal.Vector(
        item["x"], item["y"], grounded_z(mesh, scale, floor_top)
    ), False, False)
    tag_earth_greenbelt(actor)
    return actor


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    current = world.get_path_name().split(".")[0]
    if current != MAP:
        raise RuntimeError("Open {} before placing the greenbelt; found {}".format(MAP, current))

    # All preflight checks precede the first change to the saved map.
    meshes = {}
    for key, path in MESH_PATHS.items():
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError("Missing local CC0 foliage {}. Run Tools/InstallWorldFoliage.ps1".format(path))
        if mesh.get_num_lods() < 4:
            raise RuntimeError("Foliage LODs are missing: " + path)
        meshes[key] = mesh
    actors, named = loaded_actors()
    floor = named.get("Floor")
    if not isinstance(floor, unreal.StaticMeshActor):
        raise RuntimeError("Kellen Reach port collision Floor is missing")
    origin, extent = floor.get_actor_bounds(False, False)
    floor_top = origin.z + extent.z
    plan = make_plan(origin.x, origin.y, extent.x, extent.y)
    if not any(actor.get_actor_label() == "SP Play / unified journey" for actor in actors):
        raise RuntimeError("Unified journey anchor is missing; greenbelt requires the playable port map")
    if not any(isinstance(actor, unreal.SPWorldSurface) and
               actor.get_editor_property("world_id") == "earth" for actor in actors):
        raise RuntimeError("Earth Worldworks surface is missing")
    for item in plan:
        existing = named.get(item["label"])
        if existing is not None and not isinstance(existing, unreal.StaticMeshActor):
            raise RuntimeError("Greenbelt label collision: " + item["label"])

    planned_labels = {item["label"] for item in plan}
    for item in plan:
        place_one(named, item, meshes[item["mesh"]], floor_top)

    # Remove only stale actors from this script's reserved label namespace.
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    stale = [actor for actor in actors
             if actor.get_actor_label().startswith(PREFIX) and actor.get_actor_label() not in planned_labels]
    for actor in stale:
        if not isinstance(actor, unreal.StaticMeshActor):
            raise RuntimeError("Refusing to remove a non-mesh greenbelt actor: " + actor.get_actor_label())
        actor_subsystem.destroy_actor(actor)

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save Kellen Reach greenbelt")
    saved_actors, _ = loaded_actors()
    saved_labels = {actor.get_actor_label() for actor in saved_actors if actor.get_actor_label().startswith(PREFIX)}
    if saved_labels != planned_labels:
        raise RuntimeError("Greenbelt actors missing after save")
    report = {
        "map": MAP,
        "dependency_install": "Tools/InstallWorldFoliage.ps1",
        "source_assets": MESH_PATHS,
        "ground_reference": "collision Floor top from get_actor_bounds",
        "floor_top_cm": round(floor_top, 3),
        "floor_bounds_cm": [round(origin.x - extent.x, 3), round(origin.x + extent.x, 3),
                            round(origin.y - extent.y, 3), round(origin.y + extent.y, 3)],
        "reserved_routes_cm": {name: list(rect) for name, rect in RESERVED.items()},
        "placed_count": len(plan),
        "plant_counts": {key: sum(item["mesh"] == key for item in plan) for key in MESH_PATHS},
        "generated_labels": sorted(planned_labels),
        "removed_stale_count": len(stale),
        "saved": True,
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_GREENBELT_SAVED {} plants".format(len(plan)))


main()
