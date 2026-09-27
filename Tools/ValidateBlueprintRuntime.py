"""Exercise compiled Kestrel Blueprint systems and MFD data in an unsaved editor world."""

import json
from pathlib import Path
import unreal

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
output = project / "Validation" / "blueprint-runtime.json"
checks = []


def check(name, ok, value):
    checks.append({"name": name, "passed": bool(ok), "detail": str(value)})
    if not ok:
        unreal.log_error("SPACE_PATRIOT_BLUEPRINT_RUNTIME_FAIL " + name + " " + str(value))


bp = unreal.EditorAssetLibrary.load_asset("/Game/SpacePatriot/Blueprints/BP_KestrelFlyable")
check("ship_bp_exists", bp is not None, bp)
if bp:
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(bp.generated_class(), unreal.Vector(100000, 0, 500))
    check("ship_spawns", actor is not None, actor)
    if actor:
        components = actor.get_components_by_class(unreal.SPVesselSystemsComponent)
        check("vessel_systems_attached", len(components) == 1, len(components))
        readout = actor.get_editor_property("cockpit_readout")
        check("readout_attached", readout is not None, readout)
        if readout:
            check("old_world_readout_hidden", not readout.get_editor_property("visible"),
                  readout.get_editor_property("visible"))
            actor.cycle_mfd_page(1)
            text1 = str(readout.get_editor_property("text"))
            check("page_button_changes_text", "MFD 2/4" in text1, text1)
            actor.cycle_mfd_page(1)
            text2 = str(readout.get_editor_property("text"))
            check("power_page", "MFD 3/4" in text2, text2)
            actor.cycle_mfd_page(1)
            check("mission_page", "MFD 4/4" in str(readout.get_editor_property("text")), readout.get_editor_property("text"))
            actor.cycle_mfd_page(1)
            text0 = str(readout.get_editor_property("text"))
            check("nav_page_visible_text", "MFD 1/4" in text0, text0)
            actor.adjust_mfd_brightness(-0.5)
            check("brightness_button_changes_state", actor.get_editor_property("cockpit_mfd_brightness") < 0.6,
                  actor.get_editor_property("cockpit_mfd_brightness"))
        if len(components) == 1:
            systems = components[0]
            systems.apply_preset(unreal.SPVesselProfile.TRAVEL)
            snapshot = systems.capture_state()
            check("preset_changes_vessel_state", "TRAVEL" in str(snapshot.profile), snapshot.profile)
            sim_input = actor.build_vessel_simulation_input()
            result = systems.step_systems(0.016, sim_input)
            actor.apply_vessel_simulation_output(result)
            check("tick_output_changes_readout", actor.get_editor_property("vessel_engine_factor") > 0,
                  actor.get_editor_property("vessel_engine_factor"))
        world_bp = unreal.EditorAssetLibrary.load_asset("/Game/SpacePatriot/Blueprints/BP_WorldRuntime")
        world_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(world_bp.generated_class(), unreal.Vector(101000, 0, 500)) if world_bp else None
        check("world_runtime_spawns", world_actor is not None, world_actor)
        if world_actor:
            check("world_receives_player_input", "PLAYER0" in str(world_actor.get_editor_property("auto_receive_input")),
                  world_actor.get_editor_property("auto_receive_input"))
            campaigns = world_actor.get_components_by_class(unreal.SPStoryCampaignComponent)
            check("campaign_component_attached", len(campaigns) == 1, len(campaigns))
            if len(campaigns) == 1:
                campaign = campaigns[0]
                campaign.set_editor_property("auto_save", False)
                campaign = world_actor.get_components_by_class(unreal.SPStoryCampaignComponent)[0]
                campaign.set_editor_property("save_slot", "SpacePatriotGraphValidation")
                # Editing an SCS component re-runs the construction script and replaces that
                # component instance. Always reacquire it before invoking runtime methods.
                campaign = world_actor.get_components_by_class(unreal.SPStoryCampaignComponent)[0]
                ready = campaign.initialize_campaign(False)
                started = campaign.start_quest("water") if ready else False
                check("water_quest_starts", ready and started, (ready, started))
                actor.cycle_mfd_page(1)
                actor.cycle_mfd_page(1)
                actor.cycle_mfd_page(1)
                mission_text = str(readout.get_editor_property("text"))
                check("mission_mfd_shows_live_story", started and "Water Ledger" in mission_text, mission_text)
            unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(world_actor)
        unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)

summary = {"passed": sum(c["passed"] for c in checks), "failed": sum(not c["passed"] for c in checks), "checks": checks}
output.write_text(json.dumps(summary, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_BLUEPRINT_RUNTIME " + str(summary["passed"]) + " passed, " + str(summary["failed"]) + " failed")
if summary["failed"]:
    raise RuntimeError("Blueprint runtime validation failed; see Validation/blueprint-runtime.json")
