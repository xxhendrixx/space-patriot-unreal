"""Run in Unreal Editor after the SpacePatriotUnreal C++ module compiles."""
import unreal


ROOT = "/Game/SpacePatriot"
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def make_blueprint(name, class_path):
    package_path = ROOT + "/Blueprints"
    unreal.EditorAssetLibrary.make_directory(package_path)
    existing = unreal.EditorAssetLibrary.load_asset(package_path + "/" + name)
    if existing:
        unreal.log("Keeping existing Blueprint: " + name)
        return existing
    parent = unreal.load_class(None, class_path)
    if not parent:
        raise RuntimeError("Missing compiled parent class: " + class_path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    asset = ASSET_TOOLS.create_asset(name, package_path, unreal.Blueprint, factory)
    if not asset:
        raise RuntimeError("Could not create Blueprint: " + name)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log("Created Blueprint: " + package_path + "/" + name)
    return asset


blueprints = [
    ("BP_WorldRuntime", "/Script/SpacePatriotUnreal.SPWorldRuntime"),
    ("BP_PlayerShip", "/Script/SpacePatriotUnreal.SPPlayerShipPawn"),
    ("BP_WildlifeEncounter", "/Script/SpacePatriotUnreal.SPWildlifeEncounter"),
    ("BP_CitizenAgent", "/Script/SpacePatriotUnreal.SPCitizenAgent"),
    ("BP_CockpitMFD", "/Script/SpacePatriotUnreal.SPCockpitMFD"),
    ("BP_SystemsComponent", "/Script/SpacePatriotUnreal.SpacePatriotSystemsComponent"),
    ("BP_SpacePatriotGameMode", "/Script/Engine.GameModeBase"),
]
created = [make_blueprint(name, class_path) for name, class_path in blueprints]
game_mode_default = unreal.get_default_object(created[-1].generated_class())
game_mode_default.set_editor_property("default_pawn_class", created[1].generated_class())
unreal.EditorAssetLibrary.save_loaded_asset(created[-1])

map_path = ROOT + "/Maps/L_SpacePatriotPrototype"
unreal.EditorAssetLibrary.make_directory(ROOT + "/Maps")
if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
    unreal.EditorLevelLibrary.new_level(map_path)
    world_class = created[0].generated_class()
    unreal.EditorLevelLibrary.spawn_actor_from_class(world_class, unreal.Vector(0, 0, 0))
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("Created prototype map and placed BP_WorldRuntime")
else:
    unreal.log("Keeping existing prototype map: " + map_path)

unreal.EditorAssetLibrary.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
unreal.log("Space Patriot Blueprint handoff is ready. Add meshes, animation, Enhanced Input, and BP event graphs next.")
