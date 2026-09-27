"""Repair the saved port ship interaction without rebuilding the level.

Run against L_KellenReachWalk in UnrealEditor-Cmd while the interactive Editor
is closed. The physical Kestrel remains spawned by SPPlayLoopDirector in PIE.
"""

import json
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
world = unreal.EditorLevelLibrary.get_editor_world()
if world.get_path_name().split(".")[0] != MAP:
    raise RuntimeError("Load " + MAP + " before repairing boarding")

actors = unreal.EditorLevelLibrary.get_all_level_actors()
directors = [actor for actor in actors if isinstance(actor, unreal.SPPlayLoopDirector)]
starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
displays = [actor for actor in actors if actor.get_actor_label() ==
            "SP Ship / Viktor cruiser display"]
if len(directors) != 1 or len(starts) != 1 or len(displays) != 1:
    raise RuntimeError("Expected one journey director, player start, and editor cruiser display")

director = directors[0]
director.set_editor_property("boarding_offset_local", unreal.Vector(-300, -800, -170))
director.set_editor_property("boarding_range_cm", 1200.0)
display = displays[0]
tags = list(display.get_editor_property("tags"))
if "SP_DisplayOnly" not in [str(tag) for tag in tags]:
    tags.append(unreal.Name("SP_DisplayOnly"))
    display.set_editor_property("tags", tags)

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Unreal did not save the repaired play map")

board = director.get_editor_property("parked_ship_location") + director.get_editor_property("boarding_offset_local")
distance = (starts[0].get_actor_location() - board).length()
report = {
    "map": MAP,
    "hatch_cm": [board.x, board.y, board.z],
    "start_to_hatch_cm": round(distance, 1),
    "boarding_range_cm": director.get_editor_property("boarding_range_cm"),
    "spawn_can_board": distance <= 1100,
    "display_tagged_editor_only": "SP_DisplayOnly" in [str(tag) for tag in tags],
}
if not report["spawn_can_board"]:
    raise RuntimeError("The player still starts outside reliable boarding range")
(PROJECT / "Data" / "BoardingAccessRepair.json").write_text(
    json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_BOARDING_ACCESS_REPAIRED " + str(report))
