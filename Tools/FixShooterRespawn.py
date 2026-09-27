"""Keep the port spawn safe and the Shooter test in its own eastern zone.

BP_ShooterPlayerController selects RED or BLUE from Team Byte and asks
GetAllActorsOfClassWithTag(PlayerStart, selected_tag) before a random pick.
The map's original PlayerStart has neither tag. A death then picks from an
empty array and attempts to spawn the shooter pawn at world origin. Earlier
map builders also placed armed AI beside the player/ship. This pass repairs
both problems on an existing map without replacing the Shooter templates.

Run with /Game/SpacePatriot/Maps/L_KellenReachWalk loaded. This is idempotent
and preserves the Shooter character/controller Blueprints and their controls.
"""

import json
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
START_LABEL = "SP Walk / player start"
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Data" / "ShooterRespawnRepair.json"


def xy_distance(a, b):
    return ((a.x - b.x) ** 2 + (a.y - b.y) ** 2) ** 0.5


def near_xy(actor, xy, tolerance=3.0):
    p = actor.get_actor_location()
    return abs(p.x - xy[0]) <= tolerance and abs(p.y - xy[1]) <= tolerance


def relocate_if_authored(named, label, accepted_xy, new_xy, floor_top, z_offset):
    actor = named.get(label)
    if actor is None:
        raise RuntimeError("Missing authored combat actor: " + label)
    if not any(near_xy(actor, xy) for xy in (*accepted_xy, new_xy)):
        raise RuntimeError("Refusing to overwrite edited location of {}: {}".format(
            label, actor.get_actor_location()))
    actor.set_actor_location(unreal.Vector(new_xy[0], new_xy[1], floor_top + z_offset),
                             False, False)
    p = actor.get_actor_location()
    return {"label": label, "position": [round(p.x, 2), round(p.y, 2), round(p.z, 2)]}


def set_nav_bounds(named, label, xyz, half_extent):
    nav = named.get(label)
    if nav is None:
        nav = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.NavMeshBoundsVolume, unreal.Vector(*xyz), unreal.Rotator())
        if nav is None:
            raise RuntimeError("Could not create " + label)
        nav.set_actor_label(label)
        named[label] = nav
    elif not isinstance(nav, unreal.NavMeshBoundsVolume):
        raise RuntimeError("Another actor uses the nav volume label: " + label)
    nav.set_actor_location(unreal.Vector(*xyz), False, False)
    nav.set_actor_scale3d(unreal.Vector(1, 1, 1))
    _, base = nav.get_actor_bounds(False, False)
    if min(base.x, base.y, base.z) <= 0:
        raise RuntimeError("Nav volume has no brush geometry: " + label)
    nav.set_actor_scale3d(unreal.Vector(half_extent[0] / base.x,
                                        half_extent[1] / base.y,
                                        half_extent[2] / base.z))
    center, measured = nav.get_actor_bounds(False, False)
    if any(abs(getattr(measured, axis) - expected) > 10
           for axis, expected in zip(("x", "y", "z"), half_extent)):
        raise RuntimeError("Nav volume bounds failed to resize: " + label)
    return {"label": label,
            "center": [round(center.x), round(center.y), round(center.z)],
            "half_extent": [round(measured.x), round(measured.y), round(measured.z)]}


def relocate_cover(named, label, xy):
    moved = []
    for name, actor in named.items():
        if not name.startswith("SP Placeholders / " + label + " / crate "):
            continue
        if not isinstance(actor, unreal.StaticMeshActor):
            raise RuntimeError("Combat cover is not a mesh: " + name)
        old = actor.get_actor_location()
        actor.set_actor_location(unreal.Vector(xy[0], xy[1], old.z), False, False)
        moved.append(name)
    if len(moved) != (2 if label == "east cover" else 3):
        raise RuntimeError("Combat cover stack is incomplete: " + label)
    return moved


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world.get_path_name().split(".")[0] != MAP:
        raise RuntimeError("Open {} before running this repair".format(MAP))
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    named = {actor.get_actor_label(): actor for actor in actors}
    starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    if len(starts) != 1 or starts[0].get_actor_label() != START_LABEL:
        raise RuntimeError("Expected exactly one authored Shooter PlayerStart")
    start = starts[0]
    floor = named.get("Floor")
    if not isinstance(floor, unreal.StaticMeshActor):
        raise RuntimeError("The Kellen Reach port floor is missing")
    origin, extent = floor.get_actor_bounds(False, False)
    floor_top = origin.z + extent.z
    spawn = unreal.Vector(-900, -1400, floor_top + 120)
    if not near_xy(start, (-4500, -900)) and not near_xy(start, (-900, -1400)):
        raise RuntimeError("Refusing to move an edited PlayerStart: " + str(start.get_actor_location()))
    combat_xy = ((6500, 3200), (7900, 2500), (6300, 5000), (8300, 4200))
    for x, y in combat_xy:
        if not (abs(x - origin.x) < extent.x - 150 and
                abs(y - origin.y) < extent.y - 150):
            raise RuntimeError("Combat zone falls outside the collision floor")

    # The team query fails if either team's tag is absent. One physical start
    # with both tags supports both teams and keeps the port boarding lane
    # readable. Keep any user-authored tags as well.
    tags = list(start.get_editor_property("tags"))
    existing_tags = {str(tag) for tag in tags}
    for team in ("RED", "BLUE"):
        if team not in existing_tags:
            tags.append(unreal.Name(team))

    # Validate a standing capsule area before changing any actor. The apron
    # and sky are broad background surfaces, not obstructions.
    obstructions = []
    for actor in actors:
        if not isinstance(actor, unreal.StaticMeshActor) or actor is floor:
            continue
        if actor.get_actor_label() in ("Kellen Reach flight apron", "SM_SkySphere"):
            continue
        if not actor.get_actor_enable_collision():
            continue
        center, half = actor.get_actor_bounds(False, False)
        if (abs(center.x - spawn.x) < half.x + 100
                and abs(center.y - spawn.y) < half.y + 100
                and center.z + half.z > floor_top + 10
                and center.z - half.z < floor_top + 220):
            obstructions.append(actor.get_actor_label())
    if obstructions:
        raise RuntimeError("Proposed PlayerStart overlaps collision bounds: " +
                           ", ".join(sorted(obstructions)))

    start.set_editor_property("tags", tags)
    start.set_actor_location(spawn, False, False)
    moved = [
        relocate_if_authored(named, "SP Walk / shootable patrol",
                             ((-2500, -900), (-6200, 3200)),
                             (6500, 3200), floor_top, 95),
        relocate_if_authored(named, "SP Placeholders / south cargo guard",
                             ((1250, -2600), (2300, -3200)),
                             (7900, 2500), floor_top, 95),
        relocate_if_authored(named, "SP Placeholders / north service guard",
                             ((-5200, 2200),),
                             (6300, 5000), floor_top, 95),
        relocate_if_authored(named, "SP Walk / AI wave spawner",
                             ((2600, 1500), (2600, 3700)),
                             (8300, 4200), floor_top, 25),
        relocate_if_authored(named, "SP Walk / rifle pickup", ((-4050, -750),),
                             (-1200, -1800), floor_top, 45),
    ]
    cover = relocate_cover(named, "east cover", (6000, 2600))
    cover += relocate_cover(named, "harbor cover", (7300, 3800))
    nav_bounds = [
        set_nav_bounds(named, "SP Walk / NavMesh bounds",
                       (-1900, -250, floor_top + 350), (3600, 3000, 500)),
        set_nav_bounds(named, "SP Walk / combat NavMesh bounds",
                       (7200, 3600, floor_top + 350), (2600, 2300, 500)),
    ]
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Unreal did not save repaired Shooter map")

    boarding = named.get("SP Play / unified journey")
    if boarding:
        board = (boarding.get_editor_property("parked_ship_location") +
                 boarding.get_editor_property("boarding_offset_local"))
        board_distance = round(xy_distance(spawn, board) / 100.0, 2)
    else:
        board_distance = None
    report = {
        "map": MAP,
        "player_start": [round(spawn.x, 2), round(spawn.y, 2), round(spawn.z, 2)],
        "player_start_tags": [str(tag) for tag in start.get_editor_property("tags")],
        "team_tags_required_by_blueprint": ["RED", "BLUE"],
        "distance_to_boarding_point_m": board_distance,
        "moved_hostiles": moved,
        "moved_combat_cover": cover,
        "nav_bounds": nav_bounds,
        "navigation_rebuild_required": "Run Tools/RebuildKellenReachNav.py in the loaded GUI Editor, then validate in a fresh commandlet.",
        "spawn_obstructions": obstructions,
        "saved": True,
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_SHOOTER_RESPAWN_REPAIRED " + str(REPORT))


main()
