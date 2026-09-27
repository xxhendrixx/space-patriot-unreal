"""Check fresh-load navigation coverage for the on-foot port playtest.

Run with L_KellenReachWalk opened in UnrealEditor-Cmd. This checks saved nav
data/path queries; it does not substitute for observing patrol AI in PIE.
"""

import json
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Data" / "KellenReachNavigationValidation.json"
EXPECTED_MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
world = unreal.EditorLevelLibrary.get_editor_world()
actors = unreal.EditorLevelLibrary.get_all_level_actors()
named = {actor.get_actor_label(): actor for actor in actors}
checks = []


def check(name, passed, detail):
    checks.append({"name": name, "passed": bool(passed), "detail": detail})
    if not passed:
        unreal.log_error("SPACE_PATRIOT_NAV_FAIL " + name + " " + str(detail))


check("walk_map_loaded", world.get_path_name().split(".")[0] == EXPECTED_MAP,
      world.get_path_name())
nav_actors = [actor for actor in actors if isinstance(actor, unreal.RecastNavMesh)]
check("recast_navmesh_present", len(nav_actors) > 0, len(nav_actors))

start = named.get("SP Walk / player start")
npc = named.get("SP Walk / shootable patrol")
spawner = named.get("SP Walk / AI wave spawner")
for label, actor in (("start", start), ("npc", npc), ("spawner", spawner)):
    point = actor.get_actor_location() if actor else None
    check(label + "_present", actor is not None,
          [point.x, point.y, point.z] if point else None)

if start and npc and spawner:
    for label, target in (("start_to_npc", npc), ("npc_to_spawner", spawner)):
        origin = start if label == "start_to_npc" else npc
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(
            world, origin.get_actor_location(), target.get_actor_location()
        )
        valid = path is not None and path.is_valid() and not path.is_partial()
        detail = {"valid": bool(valid)}
        if path is not None:
            detail.update({
                "partial": bool(path.is_partial()),
                "length_cm": round(path.get_path_length(), 1),
                "points": len(path.path_points),
            })
        check(label + "_complete", valid and detail.get("length_cm", 0) > 100
              and detail.get("points", 0) >= 2, detail)

report = {"map": EXPECTED_MAP, "checks": checks,
          "passed": sum(item["passed"] for item in checks),
          "failed": sum(not item["passed"] for item in checks)}
REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_NAV_VALIDATION {} passed {} failed".format(
    report["passed"], report["failed"]
))
if report["failed"]:
    raise RuntimeError("Saved navigation validation failed; see " + str(REPORT))
