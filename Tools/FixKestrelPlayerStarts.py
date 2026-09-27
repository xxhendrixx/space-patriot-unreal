"""Keep the authored Kestrel spawn and remove the template PlayerStart.

Run in Unreal Editor Python against the project copy whose map should be saved.
The script only changes L_KestrelFlight when both expected starts are present.
"""

import json
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KestrelFlight"
REPORT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())) / "Data" / "PlayerStartValidation.json"


def starts():
    return [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
            if isinstance(actor, unreal.PlayerStart)]


def main():
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError(f"Could not load {MAP}")
    by_label = {actor.get_actor_label(): actor for actor in starts()}
    labels = set(by_label)
    if labels not in ({"PlayerStart", "Kestrel flight start"}, {"Kestrel flight start"}):
        raise RuntimeError(f"Unexpected PlayerStarts; map left untouched: {sorted(by_label)}")
    if "PlayerStart" in by_label:
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if not actors.destroy_actor(by_label["PlayerStart"]):
            raise RuntimeError("Failed to remove template PlayerStart")
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Failed to save Kestrel map")
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Failed to reopen saved Kestrel map")
    remaining = starts()
    if len(remaining) != 1 or remaining[0].get_actor_label() != "Kestrel flight start":
        raise RuntimeError("Saved Kestrel map still has an unexpected PlayerStart")
    location = remaining[0].get_actor_location()
    report = {
        "map": MAP,
        "player_start_count": 1,
        "label": remaining[0].get_actor_label(),
        "location_cm": [location.x, location.y, location.z],
        "saved_and_reloaded": True,
    }
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_PLAYER_START_FIXED " + json.dumps(report))


main()
