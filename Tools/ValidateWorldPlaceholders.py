"""Fresh-process checks for the Kellen Reach placeholder population pass."""

import json
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
PREFIX = "SP Placeholders / "
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
PLAN = PROJECT / "Data" / "WorldPlaceholderPopulationReport.json"
OUTPUT = PROJECT / "Data" / "WorldPlaceholderValidation.json"
checks = []


def check(name, passed, detail):
    checks.append({"name": name, "passed": bool(passed), "detail": detail})
    if not passed:
        unreal.log_error("SPACE_PATRIOT_POPULATION_FAIL {} {}".format(name, detail))


world = unreal.EditorLevelLibrary.get_editor_world()
check("walk_map_loaded_fresh", world.get_path_name().split(".")[0] == MAP,
      world.get_path_name())
if not PLAN.is_file():
    raise RuntimeError("Run Tools/PopulateWorldPlaceholders.py before validation")
plan = json.loads(PLAN.read_text(encoding="utf-8"))
actors = unreal.EditorLevelLibrary.get_all_level_actors()
by_label = {actor.get_actor_label(): actor for actor in actors}
generated = [actor for actor in actors if actor.get_actor_label().startswith(PREFIX)]
expected = set(plan["generated_labels"])
observed = {actor.get_actor_label() for actor in generated}
check("all_generated_actors_saved", observed == expected and len(generated) == len(expected),
      {"expected": len(expected), "observed": len(generated),
       "missing": sorted(expected - observed), "unexpected": sorted(observed - expected)})
check("all_earth_only_tagged", all(
    "SP_EarthOnly" in [str(tag) for tag in actor.get_editor_property("tags")]
    for actor in generated), len(generated))

meshes = [actor for actor in generated if isinstance(actor, unreal.StaticMeshActor)]
missing_materials = []
for actor in meshes:
    mesh = actor.static_mesh_component.get_editor_property("static_mesh")
    material = actor.static_mesh_component.get_material(0)
    if mesh is None or material is None or "M_PH_" not in material.get_path_name():
        missing_materials.append(actor.get_actor_label())
expected_mesh_count = plan["generated_actor_count"] - plan["zones"]["hostile patrols"]
check("textured_placeholder_meshes_loaded", len(meshes) == expected_mesh_count and not missing_materials,
      {"count": len(meshes), "missing_pbr": missing_materials})

lane_intersections = []
boarding_intersections = []
for actor in meshes:
    origin, extent = actor.get_actor_bounds(False, False)
    xmin, xmax = origin.x - extent.x, origin.x + extent.x
    ymin, ymax = origin.y - extent.y, origin.y + extent.y
    if xmax >= -4500 and xmin <= 1000 and ymax >= -1500 and ymin <= 1500:
        lane_intersections.append(actor.get_actor_label())
    if xmax >= -800 and xmin <= 200 and ymax >= -1550 and ymin <= -550:
        boarding_intersections.append(actor.get_actor_label())
check("player_to_ship_lane_clear", not lane_intersections, lane_intersections)
check("boarding_marker_clear", not boarding_intersections, boarding_intersections)

patrols = [actor for actor in generated if "guard" in actor.get_actor_label()]
check("two_shooter_patrols", len(patrols) == 2 and all(
    "BP_ShooterNPC" in actor.get_class().get_path_name() for actor in patrols),
    [actor.get_class().get_path_name() for actor in patrols])
check("patrol_ai_enabled", all(
    actor.get_editor_property("auto_possess_ai") ==
    unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED for actor in patrols),
    len(patrols))
check("patrols_in_east_combat_zone", len(patrols) == 2 and all(
    actor.get_actor_location().x >= 5000 and actor.get_actor_location().y >= 2000
    for actor in patrols),
    [(actor.get_actor_label(), round(actor.get_actor_location().x),
      round(actor.get_actor_location().y)) for actor in patrols])
combat_cover = [actor for actor in meshes if
                "/ east cover /" in actor.get_actor_label() or
                "/ harbor cover /" in actor.get_actor_label()]
check("combat_cover_in_east_zone", len(combat_cover) == 5 and all(
    actor.get_actor_location().x >= 5000 for actor in combat_cover),
    [(actor.get_actor_label(), round(actor.get_actor_location().x))
     for actor in combat_cover])

report = {"map": MAP, "passed": sum(item["passed"] for item in checks),
          "failed": sum(not item["passed"] for item in checks), "checks": checks,
          "scope": "Saved asset/actor layout only; live PIE combat and pathing are separate checks."}
OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_POPULATION_VALIDATION {} passed {} failed".format(
    report["passed"], report["failed"]))
if report["failed"]:
    raise RuntimeError("Population validation failed; see " + str(OUTPUT))
