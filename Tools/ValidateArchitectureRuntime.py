"""Run headless in Unreal Editor after compiling SPArchitecture.cpp.

UnrealEditor-Cmd.exe SpacePatriotUnreal.uproject -ExecutePythonScript=Tools/ValidateArchitectureRuntime.py
"""

import json
from pathlib import Path

import unreal


project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
report_path = project / "Data" / "Architecture" / "ArchitectureRuntimeValidation.json"
checks = []


def check(name, passed, detail=""):
    checks.append({"name": name, "passed": bool(passed), "detail": str(detail)})
    if not passed:
        unreal.log_error("SP_ARCHITECTURE_FAIL " + name + " " + str(detail))


def route(actor, start, goal):
    result = actor.find_room_route(start, goal)
    if isinstance(result, tuple):
        return bool(result[0]), list(result[1])
    if result is not None:
        return bool(result), list(result)
    return False, []


architecture_class = unreal.load_class(None, "/Script/SpacePatriotUnreal.SPArchitecture")
check("native_architecture_class", architecture_class is not None)
check("test_map_loads", unreal.EditorLevelLibrary.load_level("/Game/SpacePatriot/Maps/L_KestrelFlight"))
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(architecture_class, unreal.Vector(0, 0, 20000)) if architecture_class else None
check("architecture_actor_spawns", actor is not None)
if actor:
    check("source_family_9_loads", actor.load_interior_family(9), actor.get_editor_property("last_build_error"))
    check("four_decks", len(actor.get_editor_property("decks")) == 4)
    check("fifty_three_rooms", len(actor.get_editor_property("rooms")) == 53)
    check("fifty_two_doors", len(actor.get_editor_property("doors")) == 52)
    check("one_hundred_seventy_six_fixtures", actor.get_editor_property("fixture_count") == 176)
    count = actor.get_editor_property("structure_instances")
    check("bounded_structure_instances", 0 < count < 2048, count)
    check("corridor_occupied", actor.can_occupy_local_location(unreal.Vector(0, -3000, 0)))
    check("outside_not_occupied", not actor.can_occupy_local_location(unreal.Vector(1300, -3000, 0)))
    reachable, ids = route(actor, "bridge", "cargo-3-0--1")
    check("route_between_flight_and_freight_decks", reachable and ids[0] == "bridge" and ids[-1] == "cargo-3-0--1", ids)
    check("source_bulkhead_starts_open", actor.is_door_open("cargo-3-0--1-door"))
    check("bulkhead_closes", actor.set_door_open("cargo-3-0--1-door", False))
    reachable, _ = route(actor, "bridge", "cargo-3-0--1")
    check("closed_bulkhead_blocks_room_route", not reachable)
    check("bulkhead_reopens", actor.set_door_open("cargo-3-0--1-door", True))
    actor.set_lift_power(False)
    check("unpowered_lift_refuses_request", not actor.request_lift_to_deck(3))
    reachable, _ = route(actor, "bridge", "cargo-3-0--1")
    check("unpowered_lift_blocks_cross_deck_route", not reachable)
    actor.set_lift_power(True)
    check("powered_lift_accepts_request", actor.request_lift_to_deck(3))
    actor.advance_lift(10.0)
    check("lift_arrives_at_freight_deck", actor.get_editor_property("lift_deck") == 3 and not actor.get_editor_property("lift_moving"))
    check("small_family_loads", actor.load_interior_family(0), actor.get_editor_property("last_build_error"))
    check("small_family_source_counts", len(actor.get_editor_property("rooms")) == 2 and len(actor.get_editor_property("decks")) == 1)
    unreal.EditorLevelLibrary.destroy_actor(actor)

result = {"checks": checks, "passed": sum(c["passed"] for c in checks), "failed": sum(not c["passed"] for c in checks)}
report_path.write_text(json.dumps(result, indent=2), encoding="utf-8")
unreal.log(f"SP_ARCHITECTURE_VALIDATION {result['passed']} passed, {result['failed']} failed")
if result["failed"]:
    raise RuntimeError("SPArchitecture validation failed: " + str(report_path))
