"""Run all fresh-map checks for the current playable Unreal slice in one load.

Load L_KellenReachWalk in UnrealEditor-Cmd, then execute this script. It writes
a machine-readable rollup because Unreal's Python execution may leave a zero
process exit code even when one inner validation reports a failure.
"""

import json
from pathlib import Path
import runpy

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
TOOLS = PROJECT / "Tools"
OUTPUT = PROJECT / "Data" / "CohesiveWorldValidation.json"
SCRIPTS = (
    ("walk", "ValidateKellenReachWalk.py", "KellenReachWalkValidation.json"),
    ("navigation", "ValidateKellenReachNavigation.py", "KellenReachNavigationValidation.json"),
    ("population", "ValidateWorldPlaceholders.py", "WorldPlaceholderValidation.json"),
    ("play_loop", "ValidateCohesivePlay.py", "CohesivePlayValidation.json"),
    ("respawn", "ValidateShooterRespawn.py", "ShooterRespawnValidation.json"),
    ("ship_display", "ValidateViktorShipDisplay.py", "ShipDisplayValidation.json"),
)

results = []
for name, script_name, report_name in SCRIPTS:
    report = PROJECT / "Data" / report_name
    if report.exists():
        report.unlink()
    error = ""
    try:
        runpy.run_path(str(TOOLS / script_name), run_name="__main__")
    except Exception as exc:
        error = str(exc)
    data = json.loads(report.read_text(encoding="utf-8")) if report.exists() else {}
    failed = int(data.get("failed", 1))
    results.append({"name": name, "passed": int(data.get("passed", 0)),
                    "failed": failed, "report": report_name, "error": error})
    if error or failed:
        unreal.log_error("SPACE_PATRIOT_COHESIVE_WORLD_FAIL {} {} {}".format(
            name, failed, error))

summary = {"map": "/Game/SpacePatriot/Maps/L_KellenReachWalk",
           "passed": sum(row["passed"] for row in results),
           "failed": sum(row["failed"] for row in results), "suites": results}
OUTPUT.write_text(json.dumps(summary, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_COHESIVE_WORLD {} passed, {} failed".format(
    summary["passed"], summary["failed"]))
if summary["failed"]:
    raise RuntimeError("Cohesive world validation failed; inspect " + str(OUTPUT))
