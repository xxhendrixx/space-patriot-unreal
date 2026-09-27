"""Check that the native six-axis pawn is loaded and its public flight controls work."""

import json
from pathlib import Path
import unreal


project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
report_path = project / "Data" / "FlightPawnValidation.json"
checks = []


def check(name, ok, detail=""):
    checks.append({"name": name, "passed": bool(ok), "detail": str(detail)})
    if not ok:
        unreal.log_error("SP_FLIGHT_FAIL " + name + " " + str(detail))


flight_class = unreal.load_class(None, "/Script/SpacePatriotUnreal.SPFlightPawn")
check("native_pawn_class", flight_class is not None)
check("map_loads", unreal.EditorLevelLibrary.load_level("/Game/SpacePatriot/Maps/L_KestrelFlight"))
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
    flight_class, unreal.Vector(0, 0, 5000), unreal.Rotator(0, 0, 0)
) if flight_class else None
check("native_pawn_spawns", actor is not None)

if actor:
    before = actor.get_flight_telemetry()
    check("default_gear_down", before.gear_down)
    check("default_assist_on", before.flight_assist)
    actor.set_gear_down(False)
    actor.set_flight_assist(False)
    after = actor.get_flight_telemetry()
    check("gear_can_retract", not after.gear_down)
    check("assist_can_decouple", not after.flight_assist)
    actor.set_throttle_limit(10.0)
    check("throttle_limit_clamped", abs(actor.get_flight_telemetry().throttle_percent - 300.0) < 0.001)
    actor.set_editor_property("flying", False)
    origin = actor.get_actor_location()
    check("grounded_launch", actor.launch())
    check("launch_moves_up", actor.get_actor_location().z > origin.z)
    check("launch_restores_flight", actor.get_flight_telemetry().flying)
    unreal.EditorLevelLibrary.destroy_actor(actor)

result = {"checks": checks, "passed": sum(c["passed"] for c in checks),
          "failed": sum(not c["passed"] for c in checks)}
report_path.write_text(json.dumps(result, indent=2), encoding="utf-8")
unreal.log("SP_FLIGHT_VALIDATION " + str(result["passed"]) + " passed, " + str(result["failed"]) + " failed")
if result["failed"]:
    raise RuntimeError("SPFlightPawn validation failed: " + str(report_path))
