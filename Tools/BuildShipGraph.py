"""Build and verify real K2 Event Graphs in BP_KestrelFlyable.

Run in UnrealEditor-Cmd after building the SpacePatriotBlueprintTools Editor module.
The editor utility refuses to replace unknown user-authored Tick wiring.
"""

import json
from pathlib import Path
import unreal

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
output = project / "Validation" / "ship-blueprint-graph.json"
ship_build = unreal.SPBlueprintGraphTools.build_ship_graph()
ship_verify = unreal.SPBlueprintGraphTools.verify_ship_graph()
world_build = unreal.SPBlueprintGraphTools.build_world_graph()
world_verify = unreal.SPBlueprintGraphTools.verify_world_graph()
result = {"ship_build": str(ship_build), "ship_verify": str(ship_verify),
          "world_build": str(world_build), "world_verify": str(world_verify)}
output.write_text(json.dumps(result, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_SHIP_GRAPH " + json.dumps(result))
if not all((ship_build[0], ship_verify[0], world_build[0], world_verify[0])):
    raise RuntimeError("Blueprint graph build or verification failed: " + str(result))
