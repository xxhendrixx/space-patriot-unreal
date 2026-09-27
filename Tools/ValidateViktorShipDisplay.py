"""Fresh-process saved-map check for the local exterior ship display."""

import json
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
MESH = "/Game/SpacePatriot/OpenAssets/ViktorShips/Cruiser03_UE"
LABEL = "SP Ship / Viktor cruiser display"
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUTPUT = PROJECT / "Data" / "ShipDisplayValidation.json"
checks = []


def check(name, passed, detail):
    checks.append({"name": name, "passed": bool(passed), "detail": str(detail)})
    if not passed:
        unreal.log_error("SPACE_PATRIOT_SHIP_DISPLAY_FAIL {} {}".format(name, detail))


world = unreal.EditorLevelLibrary.get_editor_world()
check("walk_map_loaded", world.get_path_name().split(".")[0] == MAP, world.get_path_name())
actors = unreal.EditorLevelLibrary.get_all_level_actors()
directors = [actor for actor in actors if isinstance(actor, unreal.SPPlayLoopDirector)]
displays = [actor for actor in actors if actor.get_actor_label() == LABEL]
old = [actor.get_actor_label() for actor in actors
       if actor.get_actor_label().startswith("Kestrel K-017 / ")]
check("one_director", len(directors) == 1, len(directors))
check("one_display", len(displays) == 1 and isinstance(displays[0], unreal.StaticMeshActor)
      if displays else False, len(displays))
check("old_display_removed", not old, old)

mesh = unreal.EditorAssetLibrary.load_asset(MESH)
check("local_mesh_imported", isinstance(mesh, unreal.StaticMesh),
      mesh.get_path_name() if mesh else "missing")
if displays and isinstance(displays[0], unreal.StaticMeshActor):
    display = displays[0]
    assigned = display.static_mesh_component.get_editor_property("static_mesh")
    check("display_uses_cruiser", assigned == mesh if mesh else False,
          assigned.get_path_name() if assigned else "missing")
    check("display_hidden_in_game", bool(display.get_editor_property("hidden")),
          display.get_editor_property("hidden"))
    check("display_visible_in_editor", not display.is_hidden_ed(), display.is_hidden_ed())
    check("display_has_no_collision", not display.get_actor_enable_collision() and
          display.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION,
          (display.get_actor_enable_collision(), display.static_mesh_component.get_collision_enabled()))
    tags = [str(tag) for tag in display.get_editor_property("tags")]
    check("display_hides_off_earth", "SP_EarthOnly" in tags, tags)
    if directors:
        director = directors[0]
        target = director.get_editor_property("parked_ship_location")
        actual = display.get_actor_location()
        target_rotation = director.get_editor_property("parked_ship_rotation")
        actual_rotation = display.get_actor_rotation()
        check("display_matches_ship_spawn", all(abs(getattr(actual, axis) - getattr(target, axis)) < 0.1
              for axis in "xyz") and all(abs(getattr(actual_rotation, axis) -
              getattr(target_rotation, axis)) < 0.1 for axis in ("pitch", "yaw", "roll")),
              ((actual.x, actual.y, actual.z), (target.x, target.y, target.z)))
    else:
        check("display_matches_ship_spawn", False, "director missing")
else:
    for name in ("display_uses_cruiser", "display_hidden_in_game", "display_visible_in_editor", "display_has_no_collision",
                 "display_hides_off_earth", "display_matches_ship_spawn"):
        check(name, False, "display missing")

report = {"map": MAP, "passed": sum(item["passed"] for item in checks),
          "failed": sum(not item["passed"] for item in checks), "checks": checks}
OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_SHIP_DISPLAY_VALIDATION {} passed {} failed".format(
    report["passed"], report["failed"]))
if report["failed"]:
    raise RuntimeError("Saved ship display validation failed")
