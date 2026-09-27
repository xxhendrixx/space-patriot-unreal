"""Fresh-process validation of the saved Kellen Reach on-foot test level.

Start UnrealEditor-Cmd with /Game/SpacePatriot/Maps/L_KellenReachWalk before
this script. This checks authored assets and actors, not live keyboard/PIE.
"""

import json
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUTPUT = PROJECT / "Data" / "KellenReachWalkValidation.json"
checks = []


def check(name, passed, detail):
    checks.append({"name": name, "passed": bool(passed), "detail": str(detail)})
    if not passed:
        unreal.log_error("SPACE_PATRIOT_WALK_FAIL {} {}".format(name, detail))


def package(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    check("asset_" + path.rsplit("/", 1)[-1], asset is not None, path)
    return asset


world = unreal.EditorLevelLibrary.get_editor_world()
check("walk_map_loaded_fresh", world.get_path_name().split(".")[0] == MAP, world.get_path_name())
actors = unreal.EditorLevelLibrary.get_all_level_actors()
named = {actor.get_actor_label(): actor for actor in actors}

mode_bp = package("/Game/Variant_Shooter/Blueprints/BP_ShooterGameMode")
character_bp = package("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter")
controller_bp = package("/Game/Variant_Shooter/Blueprints/BP_ShooterPlayerController")
npc_bp = package("/Game/Variant_Shooter/Blueprints/AI/BP_ShooterNPC")
pickup_bp = package("/Game/Variant_Shooter/Blueprints/Pickups/BP_ShooterPickup")
package("/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Rifle")
package("/Game/Variant_Shooter/UI/UI_Shooter")
package("/Game/Input/IMC_Default")
package("/Game/Variant_Shooter/Input/IMC_Weapons")
package("/Game/Variant_Shooter/Input/Actions/IA_Shoot")
world_bp = package("/Game/SpacePatriot/Blueprints/BP_WorldRuntime")
table = package("/Game/Variant_Shooter/Blueprints/Pickups/DT_WeaponList")

settings = world.get_world_settings()
assigned = settings.get_editor_property("default_game_mode")
check("shooter_game_mode_override", bool(mode_bp) and assigned == mode_bp.generated_class(),
      assigned.get_path_name() if assigned else None)
if mode_bp:
    defaults = unreal.get_default_object(mode_bp.generated_class())
    pawn_class = defaults.get_editor_property("default_pawn_class")
    controller_class = defaults.get_editor_property("player_controller_class")
    check("shooter_character_is_default_pawn", bool(character_bp) and pawn_class == character_bp.generated_class(),
          pawn_class.get_path_name() if pawn_class else None)
    check("shooter_controller_is_default", bool(controller_bp) and controller_class == controller_bp.generated_class(),
          controller_class.get_path_name() if controller_class else None)

starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
check("single_on_foot_player_start", len(starts) == 1 and starts[0].get_actor_label() == "SP Walk / player start",
      [(actor.get_actor_label(), actor.get_actor_location().z) for actor in starts])
floor = named.get("Floor")
if floor:
    floor_origin, floor_extent = floor.get_actor_bounds(False, False)
    floor_top = floor_origin.z + floor_extent.z
    check("dressed_concrete_floor", floor_extent.x >= 10000 and floor_extent.y >= 8000,
          (floor_extent.x, floor_extent.y, floor_top))
    if starts:
        start_at = starts[0].get_actor_location()
        check("player_spawn_above_floor", floor_top + 85 <= start_at.z <= floor_top + 160,
              (floor_top, start_at.z))
else:
    check("dressed_concrete_floor", False, "missing Floor")

ship_pawns = [actor for actor in actors if isinstance(actor, unreal.Pawn)]
check("no_ship_auto_possess", all(actor.get_editor_property("auto_possess_player") == unreal.AutoReceiveInput.DISABLED
                                  for actor in ship_pawns),
      [(actor.get_actor_label(), str(actor.get_editor_property("auto_possess_player"))) for actor in ship_pawns])
ship_expected = {
    "Kestrel K-017 / Wing_Port": (-190, 70, 160),
    "Kestrel K-017 / Wing_Starboard": (-440, -150, 0),
    "Kestrel K-017 / Drive_Port": (0, -180, 0),
    "Kestrel K-017 / Drive_Starboard": (0, 190, 0),
}
for label, xyz in ship_expected.items():
    actor = named.get(label)
    actual = actor.get_actor_location() if actor else None
    check("preserved_" + label.rsplit("/", 1)[-1].strip(),
          actual is not None and all(abs(getattr(actual, axis) - xyz[index]) < 0.1
                                     for index, axis in enumerate("xyz")),
          (label, (actual.x, actual.y, actual.z) if actual else None))

world_actors = [actor for actor in actors if world_bp and actor.get_class() == world_bp.generated_class()]
check("society_runtime_preserved", len(world_actors) == 1, len(world_actors))
check("society_runtime_receives_player0_input", len(world_actors) == 1 and
      "PLAYER0" in str(world_actors[0].get_editor_property("auto_receive_input")),
      str(world_actors[0].get_editor_property("auto_receive_input")) if world_actors else "missing")
npc = named.get("SP Walk / shootable patrol")
check("shootable_npc_placed", npc is not None and npc_bp and npc.get_class() == npc_bp.generated_class(),
      npc.get_class().get_path_name() if npc else None)
if npc:
    ai_class = npc.get_editor_property("ai_controller_class")
    check("npc_has_shooter_ai", ai_class is not None and "BP_ShooterAIController" in ai_class.get_path_name(),
          ai_class.get_path_name() if ai_class else None)
    check("npc_auto_possesses_ai", npc.get_editor_property("auto_possess_ai") ==
          unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED, npc.get_editor_property("auto_possess_ai"))
    check("npc_has_hit_capsule", bool(npc.get_components_by_class(unreal.CapsuleComponent)),
          len(npc.get_components_by_class(unreal.CapsuleComponent)))

pickup = named.get("SP Walk / rifle pickup")
check("weapon_pickup_placed", pickup is not None and pickup_bp and pickup.get_class() == pickup_bp.generated_class(),
      pickup.get_class().get_path_name() if pickup else None)
pickup_meshes = []
if pickup:
    weapon_type = pickup.get_editor_property("Weapon Type")
    row_name = str(weapon_type.get_editor_property("row_name"))
    weapon_table = weapon_type.get_editor_property("data_table")
    check("pickup_grants_rifle", row_name == "Rifle" and bool(weapon_table),
          {"row_name": row_name,
           "data_table": weapon_table.get_path_name() if weapon_table else None})
    for component in pickup.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.get_editor_property("static_mesh")
        if mesh:
            pickup_meshes.append(mesh.get_path_name())
    check("pickup_has_visual_mesh", len(pickup_meshes) > 0, pickup_meshes)
    check("pickup_has_overlap_sphere", bool(pickup.get_components_by_class(unreal.SphereComponent)),
          len(pickup.get_components_by_class(unreal.SphereComponent)))

spawner = named.get("SP Walk / AI wave spawner")
check("arena_ai_spawner_placed", spawner is not None and "BP_ShooterNPCSpawner" in
      spawner.get_class().get_path_name() if spawner else False,
      spawner.get_class().get_path_name() if spawner else None)
nav = named.get("SP Walk / NavMesh bounds")
if nav and isinstance(nav, unreal.NavMeshBoundsVolume):
    _, extent = nav.get_actor_bounds(False, False)
    check("navmesh_bounds_cover_route", extent.x >= 4500 and extent.y >= 3500,
          (extent.x, extent.y, extent.z))
else:
    check("navmesh_bounds_cover_route", False, "missing NavMeshBoundsVolume")

if table:
    rows = [str(row) for row in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    check("rifle_in_weapon_table", any("rifle" in row.lower() for row in rows), rows)

registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions()
ui_dependency_owners = []
for path in ("/Game/Variant_Shooter/Blueprints/BP_ShooterGameMode",
             "/Game/Variant_Shooter/Blueprints/BP_ShooterPlayerController",
             "/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter"):
    deps = [str(item) for item in registry.get_dependencies(path, options) or []]
    if "/Game/Variant_Shooter/UI/UI_Shooter" in deps:
        ui_dependency_owners.append(path)
check("shooter_hud_referenced_by_gameplay", len(ui_dependency_owners) > 0, ui_dependency_owners)

report = {
    "map": MAP,
    "fresh_process_reload": True,
    "passed": sum(item["passed"] for item in checks),
    "failed": sum(not item["passed"] for item in checks),
    "checks": checks,
    "pickup_meshes": pickup_meshes,
    "ui_dependency_owners": ui_dependency_owners,
    "limits": "Saved actor/asset validation only. The user's Editor PIE keyboard, weapon damage, pickup overlap and AI navigation remain untested.",
}
OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_WALK_VALIDATION {} passed {} failed".format(report["passed"], report["failed"]))
if report["failed"]:
    raise RuntimeError("Walk map validation failed; see Data/KellenReachWalkValidation.json")
