"""Replace only the eight legacy editor display pieces in L_KellenReachWalk.

Run after InstallViktorHeroShip.ps1, with L_KellenReachWalk loaded in
UnrealEditor-Cmd. The display is visible in the Editor, hidden/no-collision in
Play. SPPlayLoopDirector separately spawns the playable ASPFlightPawn.
"""

import json
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
MESH = "/Game/SpacePatriot/OpenAssets/ViktorShips/Cruiser03_UE"
LABEL = "SP Ship / Viktor cruiser display"
LEGACY_PREFIX = "Kestrel K-017 / "
LEGACY_PARTS = {
    LEGACY_PREFIX + part for part in (
        "Hull", "Wing_Port", "Wing_Starboard", "Drive_Port", "Drive_Starboard",
        "Gear_Nose", "Gear_Port", "Gear_Starboard"
    )
}
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Data" / "ShipDisplayIntegration.json"


def same_vector(a, b, tolerance=0.1):
    return all(abs(getattr(a, axis) - getattr(b, axis)) <= tolerance for axis in "xyz")


def same_rotator(a, b, tolerance=0.1):
    return all(abs(getattr(a, axis) - getattr(b, axis)) <= tolerance
               for axis in ("pitch", "yaw", "roll"))


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world.get_path_name().split(".")[0] != MAP:
        raise RuntimeError("Load " + MAP + " before replacing the Editor display ship")
    mesh = unreal.EditorAssetLibrary.load_asset(MESH)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Install the local Viktor ship dependency first: " + MESH)

    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    directors = [actor for actor in actors if isinstance(actor, unreal.SPPlayLoopDirector)]
    if len(directors) != 1:
        raise RuntimeError("Expected exactly one unified play director")
    director = directors[0]
    parked = director.get_editor_property("parked_ship_location")
    rotation = director.get_editor_property("parked_ship_rotation")
    named = {}
    for actor in actors:
        named.setdefault(actor.get_actor_label(), []).append(actor)
    display = named.get(LABEL, [])
    if len(display) > 1 or (display and not isinstance(display[0], unreal.StaticMeshActor)):
        raise RuntimeError("The display ship label is duplicated or occupied by another actor")

    legacy = {name: named[name] for name in named if name.startswith(LEGACY_PREFIX)}
    if not display and not legacy:
        raise RuntimeError("Neither the eight legacy display parts nor a replacement display exists")
    if set(legacy) not in (set(), LEGACY_PARTS):
        raise RuntimeError("Partial or unexpected legacy ship display; refusing to delete actors: "
                           + str(sorted(legacy)))
    if any(len(found) != 1 or not isinstance(found[0], unreal.StaticMeshActor)
           for found in legacy.values()):
        raise RuntimeError("Legacy ship display labels are duplicated or no longer static meshes")
    for found in legacy.values():
        part_actor = found[0]
        part_mesh = part_actor.static_mesh_component.get_editor_property("static_mesh")
        if part_mesh is None or not part_mesh.get_path_name().startswith(
                "/Game/SpacePatriot/Ships/KestrelK017/"):
            raise RuntimeError("A legacy label has a different mesh; refusing to remove "
                               + part_actor.get_actor_label())

    if display:
        display_actor = display[0]
    else:
        display_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, parked, rotation
        )
        if display_actor is None:
            raise RuntimeError("Could not create the Editor ship display")
        display_actor.set_actor_label(LABEL)

    display_actor.static_mesh_component.set_static_mesh(mesh)
    display_actor.set_actor_location(parked, False, False)
    display_actor.set_actor_rotation(rotation, False)
    display_actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
    display_actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    display_actor.set_actor_enable_collision(False)
    display_actor.set_actor_hidden_in_game(True)
    display_actor.static_mesh_component.set_visibility(True)
    if hasattr(display_actor, "set_is_temporarily_hidden_in_editor"):
        display_actor.set_is_temporarily_hidden_in_editor(False)
    tags = [tag for tag in display_actor.get_editor_property("tags")
            if str(tag) not in ("SP_EarthOnly", "SP_DisplayOnly")]
    tags.extend((unreal.Name("SP_EarthOnly"), unreal.Name("SP_DisplayOnly")))
    display_actor.set_editor_property("tags", tags)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for found in legacy.values():
        if not actor_subsystem.destroy_actor(found[0]):
            raise RuntimeError("Could not remove old display actor " + found[0].get_actor_label())
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Unreal did not save the replacement display")

    remaining = unreal.EditorLevelLibrary.get_all_level_actors()
    if any(actor.get_actor_label().startswith(LEGACY_PREFIX) for actor in remaining):
        raise RuntimeError("A legacy ship display actor remains after saving")
    current = [actor for actor in remaining if actor.get_actor_label() == LABEL]
    if len(current) != 1 or not same_vector(current[0].get_actor_location(), parked) or not \
            same_rotator(current[0].get_actor_rotation(), rotation):
        raise RuntimeError("Saved display count or transform is wrong")
    report = {
        "map": MAP,
        "display_label": LABEL,
        "mesh": MESH,
        "removed_legacy_parts": sorted(legacy),
        "parked_location": [parked.x, parked.y, parked.z],
        "parked_rotation": [rotation.pitch, rotation.yaw, rotation.roll],
        "runtime": "The visible Editor display is hidden and noncolliding in Play; the director spawns the playable pawn.",
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_SHIP_DISPLAY_SAVED " + json.dumps(report))


main()
