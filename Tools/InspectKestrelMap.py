"""Record saved Kestrel level actor geometry before changing the environment."""

import json
from pathlib import Path

import unreal


MAP = "/Game/SpacePatriot/Maps/L_KestrelFlight"
PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Data" / "KestrelMapInventory.json"


def xyz(value):
    return [round(value.x, 2), round(value.y, 2), round(value.z, 2)]


if not unreal.EditorLevelLibrary.load_level(MAP):
    raise RuntimeError("Could not load " + MAP)

actors = []
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    row = {
        "label": actor.get_actor_label(),
        "class": actor.get_class().get_name(),
        "location_cm": xyz(actor.get_actor_location()),
        "scale": xyz(actor.get_actor_scale3d()),
    }
    if isinstance(actor, unreal.StaticMeshActor):
        component = actor.static_mesh_component
        mesh = component.get_editor_property("static_mesh")
        row["mesh"] = mesh.get_path_name() if mesh else None
        row["materials"] = [
            material.get_path_name() if material else None
            for material in component.get_materials()
        ]
        if mesh:
            bounds = mesh.get_bounds()
            row["mesh_extent_cm"] = xyz(bounds.box_extent)
    actors.append(row)

report = {"map": MAP, "actors": actors}
REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SPACE_PATRIOT_KESTREL_MAP_INVENTORY " + str(len(actors)))
