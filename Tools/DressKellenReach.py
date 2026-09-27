"""Dress the playable Kestrel port with imported, redistributable CC0 assets.

Run in the Unreal Editor after ImportPolyHaven.py. Generated actors have a
``SP Dressing /`` label, so this pass can be repeated without stacking props.
The level is saved and reloaded before writing its validation report.
"""

import json
import random
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KestrelFlight"
ROOT = "/Game/SpacePatriot/OpenAssets/PolyHaven"
CONCRETE = ROOT + "/HangarConcrete/M_PH_HangarConcreteXY200cm"
BOULDERS = [
    ROOT + "/Boulder01/boulder_01_LOD1",
    ROOT + "/NamaqualandBoulder03/namaqualand_boulder_03_1k",
    ROOT + "/NamaqualandBoulder05/namaqualand_boulder_05_1k",
]
CRATE = ROOT + "/PlasticCrate02/plastic_crate_02_1k"
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Data" / "KellenReachDressingReport.json"
PREFIX = "SP Dressing / "


def asset(path):
    loaded = unreal.EditorAssetLibrary.load_asset(path)
    if loaded is None:
        raise RuntimeError("Missing imported asset: " + path)
    return loaded


def actors_by_label():
    return {actor.get_actor_label(): actor
            for actor in unreal.EditorLevelLibrary.get_all_level_actors()}


def put_prop(existing, label, mesh, position, scale, yaw):
    actor = existing.get(label)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(*position), unreal.Rotator()
        )
        if actor is None:
            raise RuntimeError("Could not spawn " + label)
        actor.set_actor_label(label)
        existing[label] = actor
    elif not isinstance(actor, unreal.StaticMeshActor):
        raise RuntimeError("An existing non-mesh actor uses " + label)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_location(unreal.Vector(*position), False, False)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    actor.set_actor_rotation(unreal.Rotator(0, yaw, 0), False)
    return actor


def grounded_z(mesh, scale, floor_top, burial=0.0):
    bounds = mesh.get_bounds()
    local_low = bounds.origin.z - bounds.box_extent.z
    return floor_top - local_low * scale - burial


def main():
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Could not load " + MAP)
    existing = actors_by_label()
    floor = existing.get("Floor")
    apron = existing.get("Kellen Reach flight apron")
    if not isinstance(floor, unreal.StaticMeshActor) or not isinstance(apron, unreal.StaticMeshActor):
        raise RuntimeError("Expected template Floor and authored apron; map left unchanged")
    concrete = asset(CONCRETE)
    boulders = [asset(path) for path in BOULDERS]
    crate = asset(CRATE)
    # The UE template floor was only 80 m wide, leaving the 140 m runway
    # marks outside its collision and making the shoreline float. Keep its
    # original thickness while extending the port to the harbor edge.
    floor.set_actor_scale3d(unreal.Vector(30.0, 20.0, 8.0))
    floor.static_mesh_component.set_material(0, concrete)
    floor_origin, floor_extent = floor.get_actor_bounds(False, False)
    floor_top = floor_origin.z + floor_extent.z
    # The old 2.4 m apron and runway paint sat beneath the template floor.
    # Lift the markings onto the walkable plane and give the apron room for a
    # real ship. Retain its darker authored material to distinguish the pad.
    apron.set_actor_scale3d(unreal.Vector(80.0, 40.0, 0.01))
    apron.set_actor_location(unreal.Vector(0, 0, floor_top + 0.5), False, False)
    runway_markers = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
                      if isinstance(actor, unreal.StaticMeshActor)
                      and actor.get_actor_label() in ("Runway edge", "Runway seam")]
    if len(runway_markers) != 4:
        raise RuntimeError("Expected four authored runway markers")
    for marker in runway_markers:
        scale = marker.get_actor_scale3d()
        marker.set_actor_scale3d(unreal.Vector(scale.x, scale.y, 0.01))
        at = marker.get_actor_location()
        marker.set_actor_location(unreal.Vector(at.x, at.y, floor_top + 0.5), False, False)

    expected = []
    rng = random.Random(210726)
    for side in (-1, 1):
        for index in range(13):
            mesh = boulders[(index + (side + 1) // 2) % len(boulders)]
            x = -12600 + index * 2000 + rng.uniform(-520, 520)
            y = side * (8100 + rng.uniform(0, 1500))
            size = rng.uniform(0.95, 2.2)
            z = grounded_z(mesh, size, floor_top, rng.uniform(0, 25))
            label = f"{PREFIX}shore boulder {side:+d}-{index:02d}"
            put_prop(existing, label, mesh, (x, y, z), size, rng.uniform(0, 360))
            expected.append(label)

    # Cargo bins form two loose receiving/staging groups beside the launch
    # corridor. Gaps and offset stacks keep the loading lane traversable.
    for side in (-1, 1):
        for index in range(14):
            column = index % 5
            row = index // 5
            x = (3200 if side > 0 else -5200) + column * 175 + rng.uniform(-18, 18)
            y = side * (2780 + row * 170 + rng.uniform(-20, 20))
            size = rng.uniform(1.5, 2.0)
            z = grounded_z(crate, size, floor_top)
            label = f"{PREFIX}cargo {side:+d}-{index:02d}"
            put_prop(existing, label, crate, (x, y, z), size, 0 if side > 0 else 180)
            expected.append(label)

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save dressed Kestrel level")
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Could not reopen dressed Kestrel level")
    saved = actors_by_label()
    missing = [label for label in expected if label not in saved]
    if missing:
        raise RuntimeError("Generated props did not survive save: " + repr(missing))
    if saved["Floor"].static_mesh_component.get_material(0).get_path_name().split(".")[0] != CONCRETE:
        raise RuntimeError("Concrete material did not survive save on Floor")
    saved_apron = saved["Kellen Reach flight apron"]
    if abs(saved_apron.get_actor_location().z - (floor_top + 0.5)) > 1.0:
        raise RuntimeError("Apron is no longer on the walkable floor")
    saved_markers = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
                     if actor.get_actor_label() in ("Runway edge", "Runway seam")]
    if len(saved_markers) != 4 or any(abs(a.get_actor_location().z - (floor_top + 0.5)) > 1.0 for a in saved_markers):
        raise RuntimeError("Runway markers did not survive save at floor height")
    report = {
        "map": MAP,
        "concrete_material": CONCRETE,
        "shore_boulders": 26,
        "cargo_crates": 28,
        "ground_level_cm": round(floor_top, 2),
        "runway_markers_on_walkable_floor": len(saved_markers),
        "generated_labels": expected,
        "saved_and_reloaded": True,
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_KELLEN_REACH_DRESSED " + str(len(expected)))


main()
