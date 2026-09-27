"""Wire compiled Unreal gameplay classes into the committed Kestrel flight map.

Run in the primary C++ project after building the SpacePatriotUnreal module.
The script is idempotent and writes a report after saving and reopening assets.
"""

import json
from pathlib import Path
import traceback
import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
BP_ROOT = "/Game/SpacePatriot/Blueprints/"
MAP = "/Game/SpacePatriot/Maps/L_KestrelFlight"
REPORT = PROJECT / "Data" / "PlayableSystemsIntegration.json"
SHIP_PARTS = {
    "Hull_LOD0", "Wing_Port_LOD0", "Wing_Starboard_LOD0",
    "Drive_Port_LOD0", "Drive_Starboard_LOD0", "Gear_Nose_LOD0",
    "Gear_Port_LOD0", "Gear_Starboard_LOD0",
}
SUBSYSTEM = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary


def load_bp(name):
    bp = unreal.EditorAssetLibrary.load_asset(BP_ROOT + name)
    if bp is None:
        raise RuntimeError("Missing Blueprint " + name)
    return bp


def components(bp):
    found = []
    for handle in SUBSYSTEM.k2_gather_subobject_data_for_blueprint(bp):
        data = LIB.get_data(handle)
        obj = LIB.get_object_for_blueprint(data, bp)
        found.append((handle, data, obj))
    return found


def wire_ship():
    bp = load_bp("BP_KestrelFlyable")
    flight_class = unreal.SPFlightPawn.static_class()
    if unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp) != flight_class:
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, flight_class)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    parts = {}
    for _, _, obj in components(bp):
        if not isinstance(obj, unreal.StaticMeshComponent):
            continue
        mesh = obj.get_editor_property("static_mesh")
        mesh_name = mesh.get_name() if mesh else ""
        if mesh_name in SHIP_PARTS:
            obj.set_editor_property("relative_location", unreal.Vector(0, 0, 0))
            obj.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            parts[mesh_name] = obj.get_name()
        elif obj.get_name() == "MeshComponent0":
            obj.set_editor_property("hidden_in_game", True)
            obj.set_editor_property("visible", False)
    if set(parts) != SHIP_PARTS:
        raise RuntimeError("Ship Blueprint parts missing: " + repr(sorted(SHIP_PARTS - set(parts))))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return {"parent": unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_path_name(),
            "part_component_count": len(parts), "part_components": parts}


def add_component(bp, component_class):
    existing = [obj for _, _, obj in components(bp)
                if obj is not None and obj.get_class() == component_class]
    if existing:
        if len(existing) != 1:
            raise RuntimeError("Duplicate " + str(component_class))
        return existing[0].get_name()
    root = next((handle for handle, data, _ in components(bp) if LIB.is_root_actor(data)), None)
    if root is None:
        raise RuntimeError("Could not find Blueprint actor root")
    params = unreal.AddNewSubobjectParams()
    params.set_editor_property("parent_handle", root)
    params.set_editor_property("new_class", component_class)
    params.set_editor_property("blueprint_context", bp)
    handle, reason = SUBSYSTEM.add_new_subobject(params)
    obj = LIB.get_object_for_blueprint(LIB.get_data(handle), bp)
    if obj is None or obj.get_class() != component_class:
        raise RuntimeError("Could not add " + str(component_class) + ": " + str(reason))
    return obj.get_name()


def wire_runtime():
    bp = load_bp("BP_WorldRuntime")
    society = add_component(bp, unreal.SPSocietySimulationComponent.static_class())
    campaign = add_component(bp, unreal.SPStoryCampaignComponent.static_class())
    survey = add_component(bp, unreal.SPFieldSurveyComponent.static_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return {"society_component": society, "campaign_component": campaign,
            "field_survey_component": survey}


def wire_map():
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Could not load " + MAP)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    display = [a for a in actors if a.get_actor_label().startswith("Kestrel K-017 / ")]
    if len(display) != 8:
        raise RuntimeError("Expected eight Editor display ship parts, got " + str(len(display)))
    for actor in display:
        actor.set_actor_hidden_in_game(True)
    starts = [a for a in actors if isinstance(a, unreal.PlayerStart)
              and a.get_actor_label() == "Kestrel flight start"]
    if len(starts) != 1:
        raise RuntimeError("Expected one Kestrel PlayerStart, got " + str(len(starts)))
    starts[0].set_actor_location(unreal.Vector(0, 0, 250), False, False)
    runtimes = [a for a in actors if "BP_WorldRuntime" in a.get_class().get_name()]
    if len(runtimes) != 1:
        raise RuntimeError("Expected one world runtime actor, got " + str(len(runtimes)))
    unreal.EditorLevelLibrary.save_current_level()
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Could not reopen saved flight map")
    reloaded = unreal.EditorLevelLibrary.get_all_level_actors()
    display_reloaded = [a for a in reloaded if a.get_actor_label().startswith("Kestrel K-017 / ")]
    hidden = [a for a in display_reloaded if a.get_editor_property("hidden")]
    display_count = len(display_reloaded)
    if len(hidden) != 8:
        raise RuntimeError("Display ship is still visible during Play")
    return {"map": MAP, "display_ship_parts": display_count,
            "player_start_cm": [0, 0, 250], "runtime_actor_count": len(runtimes),
            "display_parts_hidden_in_game": len(hidden)}


def main():
    result = {"ship": wire_ship(), "runtime": wire_runtime(), "level": wire_map()}
    REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_PLAYABLE_SYSTEMS_INTEGRATED " + json.dumps(result))


try:
    main()
except Exception:
    REPORT.write_text(traceback.format_exc(), encoding="utf-8")
    raise
