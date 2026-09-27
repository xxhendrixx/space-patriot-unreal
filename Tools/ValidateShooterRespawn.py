"""Fresh-map validation for tagged Shooter respawns and a safe port start.

Run with /Game/SpacePatriot/Maps/L_KellenReachWalk loaded. Static structure
checks do not replace a live death/respawn check in Play In Editor.
"""

import json
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUTPUT = PROJECT / "Data" / "ShooterRespawnValidation.json"
checks = []


def check(name, passed, detail):
    checks.append({"name": name, "passed": bool(passed), "detail": detail})
    if not passed:
        unreal.log_error("SPACE_PATRIOT_RESPAWN_FAIL {} {}".format(name, detail))


def distance_xy(a, b):
    return ((a.x - b.x) ** 2 + (a.y - b.y) ** 2) ** 0.5


world = unreal.EditorLevelLibrary.get_editor_world()
check("unified_walk_map_loaded", world.get_path_name().split(".")[0] == MAP,
      world.get_path_name())
actors = unreal.EditorLevelLibrary.get_all_level_actors()
named = {actor.get_actor_label(): actor for actor in actors}
starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
check("one_shooter_player_start", len(starts) == 1 and
      starts[0].get_actor_label() == "SP Walk / player start" if starts else False,
      [actor.get_actor_label() for actor in starts])
start = starts[0] if len(starts) == 1 else None
floor = named.get("Floor")
check("port_floor_present", isinstance(floor, unreal.StaticMeshActor),
      floor.get_actor_label() if floor else "missing")
if start and floor:
    origin, extent = floor.get_actor_bounds(False, False)
    floor_top = origin.z + extent.z
    start_pos = start.get_actor_location()
    tags = {str(tag) for tag in start.get_editor_property("tags")}
    check("both_team_respawn_tags", {"RED", "BLUE"} <= tags, sorted(tags))
    check("start_near_ship_hatch", abs(start_pos.x + 900) < 3 and
          abs(start_pos.y + 1400) < 3 and abs(start_pos.z - (floor_top + 120)) < 3,
          [round(start_pos.x, 2), round(start_pos.y, 2), round(start_pos.z, 2)])
    pickup = named.get("SP Walk / rifle pickup")
    check("rifle_accessible_before_boarding", pickup is not None and
          distance_xy(start_pos, pickup.get_actor_location()) <= 600 if pickup else False,
          round(distance_xy(start_pos, pickup.get_actor_location()), 2) if pickup else "missing")
    director = named.get("SP Play / unified journey")
    if director:
        board = director.get_editor_property("parked_ship_location") + \
            director.get_editor_property("boarding_offset_local")
        board_distance = distance_xy(start_pos, board)
        board_range = director.get_editor_property("boarding_range_cm")
        check("ship_boarding_point_reachable", 450 <= board_distance <= board_range - 100,
              round(board_distance, 2))
    else:
        check("ship_boarding_point_reachable", False, "missing unified journey")

    safe_distances = {
        "SP Walk / shootable patrol": 7000,
        "SP Placeholders / south cargo guard": 7000,
        "SP Placeholders / north service guard": 7000,
        "SP Walk / AI wave spawner": 7000,
    }
    for label, minimum in safe_distances.items():
        actor = named.get(label)
        distance = distance_xy(start_pos, actor.get_actor_location()) if actor else None
        check("threat_spacing_" + label.replace(" ", "_").replace("/", ""),
              distance is not None and distance >= minimum,
              {"distance_cm": round(distance, 2) if distance else None,
               "minimum_cm": minimum})

    port_nav = named.get("SP Walk / NavMesh bounds")
    combat_nav = named.get("SP Walk / combat NavMesh bounds")
    if isinstance(port_nav, unreal.NavMeshBoundsVolume) and isinstance(combat_nav, unreal.NavMeshBoundsVolume):
        port_center, port_half = port_nav.get_actor_bounds(False, False)
        combat_center, combat_half = combat_nav.get_actor_bounds(False, False)
        nav_gap = combat_center.x - combat_half.x - (port_center.x + port_half.x)
        check("safe_port_combat_nav_gap", nav_gap >= 2000, round(nav_gap, 2))
        check("start_inside_port_nav", abs(start_pos.x - port_center.x) < port_half.x - 100 and
              abs(start_pos.y - port_center.y) < port_half.y - 100,
              [round(port_center.x), round(port_center.y), round(port_half.x), round(port_half.y)])
        for label in safe_distances:
            actor = named.get(label)
            p = actor.get_actor_location() if actor else None
            check("threat_inside_combat_nav_" + label.replace(" ", "_").replace("/", ""),
                  p is not None and abs(p.x - combat_center.x) < combat_half.x - 100 and
                  abs(p.y - combat_center.y) < combat_half.y - 100,
                  [round(p.x), round(p.y)] if p else "missing")
    else:
        check("safe_port_combat_nav_gap", False, "two separate NavMeshBoundsVolume actors required")

    blockers = []
    for actor in actors:
        if not isinstance(actor, unreal.StaticMeshActor) or actor is floor:
            continue
        if actor.get_actor_label() in ("Kellen Reach flight apron", "SM_SkySphere"):
            continue
        if not actor.get_actor_enable_collision():
            continue
        center, half = actor.get_actor_bounds(False, False)
        if (abs(center.x - start_pos.x) < half.x + 100
                and abs(center.y - start_pos.y) < half.y + 100
                and center.z + half.z > floor_top + 10
                and center.z - half.z < floor_top + 220):
            blockers.append(actor.get_actor_label())
    check("spawn_capsule_not_in_static_mesh", not blockers, sorted(blockers))

report = {"map": MAP, "passed": sum(x["passed"] for x in checks),
          "failed": sum(not x["passed"] for x in checks), "checks": checks,
          "scope": "Saved map and Shooter template spawn tags. Live death and re-possession remain to verify."}
OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_RESPAWN_VALIDATION {} passed {} failed".format(
    report["passed"], report["failed"]))
if report["failed"]:
    raise RuntimeError("Shooter respawn validation failed: " + str(OUTPUT))
