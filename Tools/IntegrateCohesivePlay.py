"""Wire the existing walk map into one board/fly/jump/land/egress play loop.

Run with L_KellenReachWalk loaded in UnrealEditor-Cmd. This only adds the
native play-loop director and tags Earth port dressing for world switching.
The flyable Kestrel is spawned at runtime so there is one persistent pawn.
"""

import json
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
SHIP = "/Game/SpacePatriot/Blueprints/BP_KestrelFlyable"
LABEL = "SP Play / unified journey"
EARTH_TAG = "SP_EarthOnly"
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Data" / "CohesivePlayIntegration.json"


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world.get_path_name().split(".")[0] != MAP:
        raise RuntimeError("Start the Editor commandlet with " + MAP)
    ship_bp = unreal.EditorAssetLibrary.load_asset(SHIP)
    if ship_bp is None or ship_bp.generated_class() is None:
        raise RuntimeError("Missing flyable Kestrel Blueprint")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    named = {actor.get_actor_label(): actor for actor in actors}
    floor = named.get("Floor")
    if not isinstance(floor, unreal.StaticMeshActor):
        raise RuntimeError("Dressed walkable port floor is missing")
    floor_origin, floor_extent = floor.get_actor_bounds(False, False)
    floor_top = floor_origin.z + floor_extent.z
    if floor_extent.x < 10000 or floor_extent.y < 8000:
        raise RuntimeError("The unified journey needs the full port floor")

    surfaces = [actor for actor in actors if isinstance(actor, unreal.SPWorldSurface)]
    if len(surfaces) != 1 or surfaces[0].get_editor_property("world_id") != "earth":
        raise RuntimeError("Expected one Earth Worldworks surface")

    director = named.get(LABEL)
    if director is None:
        director = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SPPlayLoopDirector, unreal.Vector(0, 0, 0), unreal.Rotator()
        )
        if director is None:
            raise RuntimeError("Could not create the native play director")
        director.set_actor_label(LABEL)
    elif not isinstance(director, unreal.SPPlayLoopDirector):
        raise RuntimeError("The play-loop label is occupied by a different actor")
    director.set_editor_property("ship_class", ship_bp.generated_class())
    director.set_editor_property("parked_ship_location", unreal.Vector(0, 0, floor_top + 200))
    director.set_editor_property("boarding_offset_local", unreal.Vector(-300, -800, -170))
    director.set_editor_property("boarding_range_cm", 1200.0)
    director.set_editor_property("auto_receive_input", unreal.AutoReceiveInput.PLAYER0)

    earth_only = []
    for actor in actors:
        label = actor.get_actor_label()
        if (isinstance(actor, unreal.StaticMeshActor) and label != "SM_SkySphere") or label in (
            "SP Walk / shootable patrol",
            "SP Walk / AI wave spawner",
            "SP Walk / rifle pickup",
        ):
            tags = list(actor.get_editor_property("tags"))
            if EARTH_TAG not in [str(tag) for tag in tags]:
                tags.append(unreal.Name(EARTH_TAG))
                actor.set_editor_property("tags", tags)
            earth_only.append(label)
        elif label == "SM_SkySphere":
            # The template's sky dome is global atmosphere dressing, not a
            # physical Kellen Reach port object. Keep it across destinations.
            tags = [tag for tag in actor.get_editor_property("tags") if str(tag) != EARTH_TAG]
            actor.set_editor_property("tags", tags)

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Unreal did not save the unified play map")
    report = {
        "map": MAP,
        "director": LABEL,
        "ship_blueprint": SHIP,
        "parked_ship_origin": [0, 0, round(floor_top + 200, 2)],
        "board_point_offset": [-300, -800, -170],
        "boarding_range_cm": 1200.0,
        "world_surface_count": len(surfaces),
        "earth_only_actors": sorted(earth_only),
        "controls": {"board_or_exit": "E", "destination": "N", "jump": "J",
                     "launch": "Space", "gear": "G", "land": "L"},
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_COHESIVE_PLAY_SAVED " + MAP)


main()
