"""Place the native, F-operated freight terminal in the currently saved port.

Run in UnrealEditor-Cmd after building the SpacePatriotUnreal Editor module.
The script is idempotent and edits only L_KellenReachWalk; it is intentionally
separate from the main play-loop map build so another agent can run it after
ship boarding/travel integration. Do not run while someone edits the map.
"""

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
LABEL = "SP Adventure / freight terminal"
REMOTE_LABEL = "SP Adventure / remote freight proxy"
EARTH_TAG = "SP_EarthOnly"
REMOTE_TAG = "SP_RemotePortProxy"
CLASS_PATH = "/Script/SpacePatriotUnreal.SPPortTerminal"


def ensure_tag(actor, tag):
    tags = list(actor.get_editor_property("tags"))
    if tag not in [str(item) for item in tags]:
        tags.append(unreal.Name(tag))
        actor.set_editor_property("tags", tags)


def ensure_terminal(actors, label, cls, location, rotation, tag):
    actor = actors.get(label)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, location, rotation)
        if actor is None:
            raise RuntimeError("Could not spawn freight terminal: " + label)
        actor.set_actor_label(label)
    elif actor.get_class().get_path_name() != cls.get_path_name():
        raise RuntimeError("A different actor uses the freight terminal label: " + label)
    actor.set_actor_location(location, False, False)
    actor.set_actor_rotation(rotation, False)
    ensure_tag(actor, tag)
    mesh = actor.get_editor_property("terminal_mesh")
    if mesh is None or mesh.get_editor_property("static_mesh") is None:
        raise RuntimeError("Local UE template placeholder mesh is missing; run InstallLocalDependencies.ps1")
    return actor


def main():
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Could not load port map " + MAP)
    cls = unreal.load_class(None, CLASS_PATH)
    if cls is None:
        raise RuntimeError("Build native Editor module first: " + CLASS_PATH)
    actors = {a.get_actor_label(): a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
    floor = actors.get("Floor")
    if not isinstance(floor, unreal.StaticMeshActor):
        raise RuntimeError("Kellen Reach concrete Floor is missing")
    origin, extent = floor.get_actor_bounds(False, False)
    floor_top = origin.z + extent.z
    ensure_terminal(actors, LABEL, cls,
                    unreal.Vector(-3470, -1500, floor_top + 78),
                    unreal.Rotator(0, 90, 0), EARTH_TAG)
    # One reusable actor follows the landed ship after every non-Earth jump.
    # Keep it below the saved level so it cannot obstruct Earth editor work;
    # its native runtime tick traces to safe terrain before revealing it.
    ensure_terminal(actors, REMOTE_LABEL, cls,
                    unreal.Vector(0, 0, -2000000), unreal.Rotator(), REMOTE_TAG)
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save port map")
    unreal.log("Earth and remote freight terminals saved. Stand within 3.5 m and press F; E remains boarding.")


if __name__ == "__main__":
    main()
