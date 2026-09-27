"""Populate the playable Kellen Reach port with locally installed placeholder assets.

Run with L_KellenReachWalk already loaded in UnrealEditor-Cmd. This pass does
not import or redistribute vendor packages. Every spawned actor has a stable
label so the pass is repeatable after the play-loop map integration.

The cargo/loading lane and route from the on-foot start to the Kestrel remain
clear. Extra combatants use the locally staged UE 5.8 Shooter template in a
separate eastern combat zone. They are temporary hostile patrols, not the
finished society simulation.
"""

import json
import random
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
ROOT = "/Game/SpacePatriot/OpenAssets/PolyHaven"
CRATE = ROOT + "/PlasticCrate02/plastic_crate_02_1k"
ROCKS = (
    ROOT + "/Boulder01/boulder_01_LOD1",
    ROOT + "/NamaqualandBoulder03/namaqualand_boulder_03_1k",
    ROOT + "/NamaqualandBoulder05/namaqualand_boulder_05_1k",
)
NPC = "/Game/Variant_Shooter/Blueprints/AI/BP_ShooterNPC"
PREFIX = "SP Placeholders / "
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Data" / "WorldPlaceholderPopulationReport.json"
RNG = random.Random(726091)


def require_asset(path, expected):
    loaded = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(loaded, expected):
        raise RuntimeError("Missing locally staged asset {} (expected {})".format(path, expected))
    return loaded


def labels():
    return {actor.get_actor_label(): actor
            for actor in unreal.EditorLevelLibrary.get_all_level_actors()}


def grounded_z(mesh, scale, floor_top, burial=0):
    bounds = mesh.get_bounds()
    return floor_top - (bounds.origin.z - bounds.box_extent.z) * scale - burial


def tag_earth(actor):
    tags = list(actor.get_editor_property("tags"))
    if "SP_EarthOnly" not in [str(tag) for tag in tags]:
        tags.append(unreal.Name("SP_EarthOnly"))
        actor.set_editor_property("tags", tags)


def put_mesh(existing, expected, label, mesh, x, y, scale, yaw, floor_top,
             bury=0, raise_cm=0):
    full_label = PREFIX + label
    actor = existing.get(full_label)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(x, y, floor_top), unreal.Rotator()
        )
        if actor is None:
            raise RuntimeError("Could not spawn " + full_label)
        actor.set_actor_label(full_label)
        existing[full_label] = actor
    elif not isinstance(actor, unreal.StaticMeshActor):
        raise RuntimeError("Label collision with non-mesh actor: " + full_label)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    actor.set_actor_location(unreal.Vector(
        x, y, grounded_z(mesh, scale, floor_top, bury) + raise_cm
    ), False, False)
    actor.set_actor_rotation(unreal.Rotator(0, yaw, 0), False)
    tag_earth(actor)
    expected.append(full_label)
    return actor


def put_patrol(existing, expected, label, npc_class, x, y, floor_top, yaw):
    full_label = PREFIX + label
    actor = existing.get(full_label)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            npc_class, unreal.Vector(x, y, floor_top + 95), unreal.Rotator(0, yaw, 0)
        )
        if actor is None:
            raise RuntimeError("Could not spawn " + full_label)
        actor.set_actor_label(full_label)
        existing[full_label] = actor
    elif actor.get_class() != npc_class:
        raise RuntimeError("Label collision with another class: " + full_label)
    actor.set_actor_location(unreal.Vector(x, y, floor_top + 95), False, False)
    actor.set_actor_rotation(unreal.Rotator(0, yaw, 0), False)
    actor.set_editor_property("auto_possess_ai", unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
    tag_earth(actor)
    expected.append(full_label)
    return actor


def cargo_stack(existing, expected, crate, label, x, y, floor_top, yaw,
                count=3, scale=1.55):
    """Place a measured stack: each crate rests on the previous one's top."""
    bounds = crate.get_bounds()
    height = 2 * bounds.box_extent.z * scale
    for index in range(count):
        put_mesh(existing, expected, "{} / crate {:02d}".format(label, index + 1),
                 crate, x, y, scale, yaw, floor_top, raise_cm=height * index)


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    current = world.get_path_name().split(".")[0]
    if current != MAP:
        raise RuntimeError("Open {} first; found {}".format(MAP, current))

    # Check dependencies before changing the level. Those packages are installed
    # by Tools/InstallLocalDependencies.ps1 on each collaborator's computer.
    crate = require_asset(CRATE, unreal.StaticMesh)
    rocks = [require_asset(path, unreal.StaticMesh) for path in ROCKS]
    npc_bp = require_asset(NPC, unreal.Blueprint)
    npc_class = npc_bp.generated_class()
    existing = labels()
    floor = existing.get("Floor")
    if not isinstance(floor, unreal.StaticMeshActor):
        raise RuntimeError("Missing collision floor in " + MAP)
    floor_origin, floor_extent = floor.get_actor_bounds(False, False)
    floor_top = floor_origin.z + floor_extent.z
    if floor_extent.x < 10000 or floor_extent.y < 8000:
        raise RuntimeError("Walkable floor is too small for planned port layout")
    for anchor in ("SP Walk / player start", "SP Walk / rifle pickup",
                   "SP Walk / shootable patrol", "SP Walk / AI wave spawner"):
        if anchor not in existing:
            raise RuntimeError("Missing playtest anchor " + anchor)

    expected = []
    zones = {}

    # Cargo storage reads as a loading operation, with two receiving rows and
    # two outbound rows. These groups sit beyond the ship access lane (y +/-
    # 1,500 cm) and use actual textured crate meshes rather than colored cubes.
    for zone, base_x, base_y, facing in (
        ("north receiving", -6900, 4300, 0),
        ("south receiving", -7000, -4550, 180),
        ("north outbound", 4500, 4550, 180),
        ("south outbound", 4700, -4600, 0),
    ):
        start = len(expected)
        for row in range(2):
            for column in range(4):
                x = base_x + column * 205 + RNG.uniform(-16, 16)
                y = base_y + row * 225 + RNG.uniform(-15, 15)
                scale = RNG.uniform(1.4, 1.7)
                put_mesh(existing, expected,
                         "{} / bay {}-{}".format(zone, row + 1, column + 1),
                         crate, x, y, scale, facing, floor_top)
        zones[zone] = len(expected) - start

    # Two stacks mark the separate combat zone; the west stack remains port
    # dressing. None blocks the direct route to the parked ship.
    for label, x, y, count in (
        ("west cover", -1650, 2800, 2),
        ("east cover", 6000, 2600, 2),
        ("harbor cover", 7300, 3800, 3),
    ):
        cargo_stack(existing, expected, crate, label, x, y, floor_top,
                    0 if y > 0 else 90, count=count)
    zones["combat cover"] = 7

    # Shore and service road remain distinct: loose geology forms small
    # clusters off the port apron, with different silhouettes and scales.
    for bank, sign in (("north", 1), ("south", -1)):
        start = len(expected)
        for cluster in range(5):
            cx = -8000 + cluster * 3600 + RNG.uniform(-350, 350)
            cy = sign * (6200 + RNG.uniform(0, 800))
            for piece in range(3):
                mesh = rocks[(cluster + piece + (1 if sign < 0 else 0)) % len(rocks)]
                x = cx + RNG.uniform(-150, 150)
                y = cy + RNG.uniform(-165, 165)
                scale = RNG.uniform(0.75, 1.55)
                put_mesh(existing, expected,
                         "{} verge / cluster {} / stone {}".format(bank, cluster + 1, piece + 1),
                         mesh, x, y, scale, RNG.uniform(0, 360), floor_top,
                         bury=RNG.uniform(0, 15))
        zones[bank + " verge"] = len(expected) - start

    # Both are inside the eastern combat nav volume. The port and ship hatch
    # remain a safe hub while the society/job simulation is integrated.
    put_patrol(existing, expected, "south cargo guard", npc_class,
               7900, 2500, floor_top, 135)
    put_patrol(existing, expected, "north service guard", npc_class,
               6300, 5000, floor_top, 315)
    zones["hostile patrols"] = 2

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Unreal failed to save " + MAP)
    saved = labels()
    missing = sorted(set(expected) - set(saved))
    if missing:
        raise RuntimeError("Placed actors missing after save: " + repr(missing))
    untagged = [label for label in expected
                if "SP_EarthOnly" not in [str(tag) for tag in saved[label].get_editor_property("tags")]]
    if untagged:
        raise RuntimeError("Generated actors missing world tag: " + repr(untagged))
    report = {
        "map": MAP,
        "assets": [CRATE, *ROCKS, NPC],
        "external_asset_bytes_committed": 0,
        "floor_top_cm": round(floor_top, 2),
        "zones": zones,
        "generated_actor_count": len(expected),
        "earth_only_tagged_count": len(expected),
        "generated_labels": expected,
        "player_to_ship_clear_lane": "x -4500 to 1000, y -1500 to 1500",
        "limits": "Safe port dressing and a separate hostile combat test; not a complete society or planet asset pass.",
        "saved": True,
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_PORT_POPULATED {} actors".format(len(expected)))


main()
