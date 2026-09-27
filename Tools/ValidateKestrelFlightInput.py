"""Read-only saved-map check for flight input and authored ship positions."""

import json
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KestrelFlight"
ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = ROOT / "Data" / "KestrelFlightInputValidation.json"
world = unreal.EditorLevelLibrary.get_editor_world()
actors = unreal.EditorLevelLibrary.get_all_level_actors()
named = {actor.get_actor_label(): actor for actor in actors}
runtime_bp = unreal.EditorAssetLibrary.load_asset("/Game/SpacePatriot/Blueprints/BP_WorldRuntime")
runtime = [actor for actor in actors if runtime_bp and actor.get_class() == runtime_bp.generated_class()]
expected = {
    "Kestrel K-017 / Wing_Port": (-190, 70, 160),
    "Kestrel K-017 / Wing_Starboard": (-440, -150, 0),
    "Kestrel K-017 / Drive_Port": (0, -180, 0),
    "Kestrel K-017 / Drive_Starboard": (0, 190, 0),
}
positions = {}
for label, xyz in expected.items():
    actor = named.get(label)
    point = actor.get_actor_location() if actor else None
    positions[label] = [round(point.x, 2), round(point.y, 2), round(point.z, 2)] if point else None

checks = {
    "source_flight_map_loaded": world.get_path_name().split(".")[0] == MAP,
    "single_world_runtime": len(runtime) == 1,
    "world_runtime_receives_player0_input": len(runtime) == 1 and
        "PLAYER0" in str(runtime[0].get_editor_property("auto_receive_input")),
    "manual_ship_positions_preserved": all(positions[label] == list(xyz) for label, xyz in expected.items()),
}
report = {
    "map": MAP,
    "passed": sum(checks.values()),
    "failed": sum(not result for result in checks.values()),
    "checks": checks,
    "world_runtime_auto_receive_input": str(runtime[0].get_editor_property("auto_receive_input")) if runtime else None,
    "ship_positions": positions,
}
REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_FLIGHT_INPUT_VALIDATION {} passed {} failed".format(report["passed"], report["failed"]))
if report["failed"]:
    raise RuntimeError("Flight input validation failed; see " + str(REPORT))
