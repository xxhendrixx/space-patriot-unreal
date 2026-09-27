"""Fresh-load acceptance checks for the saved, single-map player journey.

Structural Editor checks cannot replace live Selected Viewport input testing.
"""

import json
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUTPUT = PROJECT / "Data" / "CohesivePlayValidation.json"
MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
checks = []


def check(name, passed, detail):
    row = {"name": name, "passed": bool(passed), "detail": str(detail)}
    checks.append(row)
    if not passed:
        unreal.log_error("SPACE_PATRIOT_COHESIVE_FAIL {} {}".format(name, detail))


world = unreal.EditorLevelLibrary.get_editor_world()
check("unified_map_loaded", world.get_path_name().split(".")[0] == MAP,
      world.get_path_name())
actors = unreal.EditorLevelLibrary.get_all_level_actors()
named = {actor.get_actor_label(): actor for actor in actors}
directors = [actor for actor in actors if isinstance(actor, unreal.SPPlayLoopDirector)]
check("one_play_loop_director", len(directors) == 1,
      [actor.get_actor_label() for actor in directors])
director = directors[0] if directors else None
ship_asset = unreal.EditorAssetLibrary.load_asset(
    "/Game/SpacePatriot/Blueprints/BP_KestrelFlyable")
check("flyable_kestrel_loaded", ship_asset is not None,
      ship_asset.get_path_name() if ship_asset else "missing")
if director:
    selected_ship = director.get_editor_property("ship_class")
    check("director_uses_real_kestrel", bool(ship_asset) and
          selected_ship == ship_asset.generated_class(),
          selected_ship.get_path_name() if selected_ship else "missing")
    check("route_component_saved", bool(director.get_components_by_class(
          unreal.SPHyperjumpRouteComponent)),
          len(director.get_components_by_class(unreal.SPHyperjumpRouteComponent)))
    check("director_input_enabled", "PLAYER0" in str(director.get_editor_property(
          "auto_receive_input")), director.get_editor_property("auto_receive_input"))
    board = director.get_editor_property("parked_ship_location") + director.get_editor_property(
        "boarding_offset_local")
    check("board_point_near_hatch", -800 <= board.x <= 200 and -1400 <= board.y <= -750
          and -20 <= board.z <= 100, (board.x, board.y, board.z))
    starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    start_gap = (starts[0].get_actor_location() - board).length() if len(starts) == 1 else float("inf")
    board_range = director.get_editor_property("boarding_range_cm")
    check("spawn_can_board_without_pixel_perfect_position", start_gap <= board_range - 100,
          {"start_to_hatch_cm": round(start_gap, 1), "range_cm": board_range})
    displays = [actor for actor in actors if "SP_DisplayOnly" in
                [str(tag) for tag in actor.get_editor_property("tags")]]
    check("display_cruiser_is_not_the_flyable_ship", len(displays) == 1 and
          displays[0].get_actor_label() == "SP Ship / Viktor cruiser display",
          [actor.get_actor_label() for actor in displays])
    obstructions = []
    for actor in actors:
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        label = actor.get_actor_label()
        if label in ("Floor", "Kellen Reach flight apron", "SM_SkySphere") or label.startswith("Kestrel K-017 /"):
            continue
        if "SP_DisplayOnly" in [str(tag) for tag in actor.get_editor_property("tags")]:
            continue
        center, extent = actor.get_actor_bounds(False, False)
        if abs(center.x - board.x) < extent.x + 180 and abs(center.y - board.y) < extent.y + 180 \
                and center.z + extent.z > board.z - 80:
            obstructions.append(label)
    check("boarding_lane_clear", not obstructions, obstructions)
else:
    for name in ("director_uses_real_kestrel", "route_component_saved", "director_input_enabled",
                 "board_point_near_hatch", "spawn_can_board_without_pixel_perfect_position",
                 "display_cruiser_is_not_the_flyable_ship", "boarding_lane_clear"):
        check(name, False, "director missing")

surfaces = [actor for actor in actors if isinstance(actor, unreal.SPWorldSurface)]
check("one_source_seeded_world_surface", len(surfaces) == 1 and
      surfaces[0].get_editor_property("world_id") == "earth" if surfaces else False,
      [actor.get_editor_property("world_id") for actor in surfaces])
earth_tags = [actor for actor in actors if "SP_EarthOnly" in
              [str(tag) for tag in actor.get_editor_property("tags")]]
check("earth_port_can_hide_after_jump", len(earth_tags) >= 80,
      len(earth_tags))
terminals = [actor for actor in actors if isinstance(actor, unreal.SPPortTerminal)]
terminal_labels = {actor.get_actor_label(): actor for actor in terminals}
check("freight_boards_on_earth_and_remote_worlds", len(terminals) == 2 and
      {"SP Adventure / freight terminal", "SP Adventure / remote freight proxy"} <= set(terminal_labels),
      sorted(terminal_labels))
earth_terminal = terminal_labels.get("SP Adventure / freight terminal")
remote_terminal = terminal_labels.get("SP Adventure / remote freight proxy")
check("earth_terminal_hides_offworld", bool(earth_terminal) and
      "SP_EarthOnly" in [str(tag) for tag in earth_terminal.get_editor_property("tags")],
      earth_terminal.get_editor_property("tags") if earth_terminal else "missing")
check("remote_terminal_survives_world_switch", bool(remote_terminal) and
      "SP_EarthOnly" not in [str(tag) for tag in remote_terminal.get_editor_property("tags")],
      remote_terminal.get_editor_property("tags") if remote_terminal else "missing")
config = (PROJECT / "Config" / "DefaultEngine.ini").read_text(encoding="utf-8")
check("game_starts_in_unified_map",
      "GameDefaultMap=/Game/SpacePatriot/Maps/L_KellenReachWalk.L_KellenReachWalk" in config,
      "DefaultEngine.ini")

result = {"map": MAP, "passed": sum(row["passed"] for row in checks),
          "failed": sum(not row["passed"] for row in checks), "checks": checks,
          "live_pie_verified": False}
OUTPUT.write_text(json.dumps(result, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_COHESIVE_VALIDATION {} / {}".format(
    result["passed"], len(checks)))
if result["failed"]:
    raise RuntimeError("Unified play map failed {} checks".format(result["failed"]))
