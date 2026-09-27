"""Create a distinct on-foot shooter playtest from the dressed Kestrel port.

The original flight map is only read. Labels make generated actors idempotent.
Run in UnrealEditor-Cmd after staging Epic's UE 5.8 Arena Shooter content.
"""

import json
from pathlib import Path

import unreal


SOURCE = "/Game/SpacePatriot/Maps/L_KestrelFlight"
TARGET = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
GAME_MODE = "/Game/Variant_Shooter/Blueprints/BP_ShooterGameMode"
NPC = "/Game/Variant_Shooter/Blueprints/AI/BP_ShooterNPC"
SPAWNER = "/Game/Variant_Shooter/Blueprints/AI/BP_ShooterNPCSpawner"
PICKUP = "/Game/Variant_Shooter/Blueprints/Pickups/BP_ShooterPickup"
WORLD_RUNTIME = "/Game/SpacePatriot/Blueprints/BP_WorldRuntime"
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Data" / "KellenReachWalkBuildReport.json"
EXPECTED_SHIP_LOCATIONS = {
    "Kestrel K-017 / Wing_Port": (-190, 70, 160),
    "Kestrel K-017 / Wing_Starboard": (-440, -150, 0),
    "Kestrel K-017 / Drive_Port": (0, -180, 0),
    "Kestrel K-017 / Drive_Starboard": (0, 190, 0),
}


def actor_location(actor):
    p = actor.get_actor_location()
    return [round(p.x, 2), round(p.y, 2), round(p.z, 2)]


def by_label():
    return {actor.get_actor_label(): actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()}


def generated_class(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None or not hasattr(asset, "generated_class"):
        raise RuntimeError("Could not load Blueprint class: " + path)
    return asset.generated_class()


def put_actor(existing, label, cls, xyz, yaw=0):
    actor = existing.get(label)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            cls, unreal.Vector(*xyz), unreal.Rotator(0, yaw, 0)
        )
        if actor is None:
            raise RuntimeError("Could not spawn " + label)
        actor.set_actor_label(label)
        existing[label] = actor
    elif actor.get_class().get_path_name() != cls.get_path_name():
        raise RuntimeError("Unexpected class for existing actor " + label)
    actor.set_actor_location(unreal.Vector(*xyz), False, False)
    actor.set_actor_rotation(unreal.Rotator(0, yaw, 0), False)
    return actor


def verify_ship_positions(actors, where):
    observed = {}
    for label, expected in EXPECTED_SHIP_LOCATIONS.items():
        actor = actors.get(label)
        if actor is None:
            raise RuntimeError("Missing manually placed ship module in " + where + ": " + label)
        location = actor_location(actor)
        observed[label] = location
        if any(abs(location[index] - expected[index]) > 0.1 for index in range(3)):
            raise RuntimeError("Manual ship position changed in {}: {} expected {}, found {}".format(
                where, label, expected, location
            ))
    return observed


def main():
    if not (PROJECT / "Data" / "KellenReachDressingReport.json").exists():
        raise RuntimeError("Dressed source map must be saved before making walk map")
    world = unreal.EditorLevelLibrary.get_editor_world()
    current = world.get_path_name().split(".")[0]
    if current != TARGET:
        raise RuntimeError("Start UnrealEditor-Cmd with {} as its map, found {}".format(TARGET, current))
    existing = by_label()
    ship_positions = verify_ship_positions(existing, "walk map")
    world_runtimes = [a for a in existing.values()
                      if a.get_class().get_path_name() == generated_class(WORLD_RUNTIME).get_path_name()]
    if len(world_runtimes) != 1:
        raise RuntimeError("Expected exactly one society/world runtime copied into walk map")
    # Story and survey actions live on this actor, not the shooter pawn. It
    # must receive Player0 input for F5/F6 to fire while on foot.
    world_runtimes[0].set_editor_property("auto_receive_input", unreal.AutoReceiveInput.PLAYER0)
    floor = existing.get("Floor")
    if not isinstance(floor, unreal.StaticMeshActor):
        raise RuntimeError("Missing walkable concrete Floor")
    floor_origin, floor_extent = floor.get_actor_bounds(False, False)
    floor_top = floor_origin.z + floor_extent.z
    if floor_extent.x < 10000 or floor_extent.y < 8000:
        raise RuntimeError("Dressed concrete floor is too small for on-foot test")

    shooter_mode = generated_class(GAME_MODE)
    settings = world.get_world_settings()
    settings.set_editor_property("default_game_mode", shooter_mode)

    # The flight level used two starts. A single on-foot start prevents the
    # arena GameMode from spawning the character inside the docked ship.
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        if isinstance(actor, unreal.PlayerStart):
            actor_subsystem.destroy_actor(actor)
        elif isinstance(actor, unreal.Pawn):
            actor.set_editor_property("auto_possess_player", unreal.AutoReceiveInput.DISABLED)
    existing = by_label()
    start = put_actor(existing, "SP Walk / player start", unreal.PlayerStart,
                      (-4500, -900, floor_top + 120), 0)
    pickup = put_actor(existing, "SP Walk / rifle pickup", generated_class(PICKUP),
                       (-4050, -750, floor_top + 45), 0)
    # The Arena template's base pickup defaults to Pistol. Its Rifle examples
    # explicitly override this DataTableRowHandle on the placed instance.
    weapon_type = pickup.get_editor_property("Weapon Type")
    weapon_type.set_editor_property("row_name", unreal.Name("Rifle"))
    pickup.set_editor_property("Weapon Type", weapon_type)
    npc = put_actor(existing, "SP Walk / shootable patrol", generated_class(NPC),
                    (-2500, -900, floor_top + 95), 180)
    npc.set_editor_property("auto_possess_ai", unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
    spawner = put_actor(existing, "SP Walk / AI wave spawner", generated_class(SPAWNER),
                        (2600, 1500, floor_top + 25), 180)

    nav = existing.get("SP Walk / NavMesh bounds")
    if nav is None:
        nav = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.NavMeshBoundsVolume, unreal.Vector(-2000, 0, floor_top + 350), unreal.Rotator()
        )
        if nav is None:
            raise RuntimeError("Could not create NavMeshBoundsVolume")
        nav.set_actor_label("SP Walk / NavMesh bounds")
    if not isinstance(nav, unreal.NavMeshBoundsVolume):
        raise RuntimeError("NavMesh label belongs to another actor class")
    nav.set_actor_location(unreal.Vector(-2000, 0, floor_top + 350), False, False)
    nav.set_actor_scale3d(unreal.Vector(1, 1, 1))
    _, initial_extent = nav.get_actor_bounds(False, False)
    if min(initial_extent.x, initial_extent.y, initial_extent.z) <= 0:
        raise RuntimeError("NavMeshBoundsVolume has no brush geometry")
    nav.set_actor_scale3d(unreal.Vector(5000 / initial_extent.x, 4000 / initial_extent.y,
                                       500 / initial_extent.z))
    _, nav_extent = nav.get_actor_bounds(False, False)
    if nav_extent.x < 4500 or nav_extent.y < 3500:
        raise RuntimeError("NavMeshBoundsVolume did not cover on-foot route")

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save walk map")
    saved = by_label()
    verify_ship_positions(saved, "saved walk map")
    if unreal.EditorLevelLibrary.get_editor_world().get_world_settings().get_editor_property(
            "default_game_mode").get_path_name() != shooter_mode.get_path_name():
        raise RuntimeError("Shooter GameMode did not survive save/reload")
    labels = (start.get_actor_label(), pickup.get_actor_label(), npc.get_actor_label(),
              spawner.get_actor_label(), nav.get_actor_label())
    if any(label not in saved for label in labels):
        raise RuntimeError("One or more on-foot actors did not survive save/reload")
    world_runtime_count = sum(a.get_class().get_path_name() == generated_class(WORLD_RUNTIME).get_path_name()
                              for a in saved.values())
    if world_runtime_count != 1:
        raise RuntimeError("World runtime was lost from walk map")
    report = {
        "source": SOURCE, "walk_map": TARGET, "floor_top_cm": round(floor_top, 2),
        "game_mode": shooter_mode.get_path_name(),
        "source_ship_positions": EXPECTED_SHIP_LOCATIONS,
        "walk_ship_positions": verify_ship_positions(saved, "final walk map"),
        "world_runtime_count": world_runtime_count,
        "player_start": actor_location(saved["SP Walk / player start"]),
        "pickup": actor_location(saved["SP Walk / rifle pickup"]),
        "pickup_weapon_type": str(saved["SP Walk / rifle pickup"].get_editor_property("Weapon Type")),
        "world_runtime_auto_receive_input": str(world_runtimes[0].get_editor_property("auto_receive_input")),
        "shootable_npc": actor_location(saved["SP Walk / shootable patrol"]),
        "ai_spawner": actor_location(saved["SP Walk / AI wave spawner"]),
        "nav_extent_cm": [round(nav_extent.x), round(nav_extent.y), round(nav_extent.z)],
        "saved": True,
        "reloaded_in_fresh_commandlet": False,
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_WALK_MAP_SAVED " + TARGET)


main()
