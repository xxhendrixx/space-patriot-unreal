"""Use the imported terrain material only on the near streamed world patch.

Run after BuildWorldTerrainPBR.py and the Kellen Reach walk map build. This
keeps the low-cost vertex-color material on each far globe. The report is
written only after both levels save and reload with the intended split.
"""

import gc
import json
from pathlib import Path

import unreal


ROOT = "/Game/SpacePatriot"
MATERIAL = ROOT + "/Materials/M_WorldTerrainPBR"
MAPS = [ROOT + "/Maps/L_KestrelFlight", ROOT + "/Maps/L_KellenReachWalk"]
REPORT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())) / "Data" / "WorldTerrainMapAssignment.json"


def world_surface():
    surfaces = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
                if isinstance(actor, unreal.SPWorldSurface)]
    if len(surfaces) != 1:
        raise RuntimeError("Expected one world surface, found " + str(len(surfaces)))
    return surfaces[0]


def main():
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL)
    if material is None or not isinstance(material, unreal.MaterialInterface):
        raise RuntimeError("Missing validated terrain material " + MATERIAL)
    results = {}
    for map_path in MAPS:
        if not unreal.EditorLevelLibrary.load_level(map_path):
            raise RuntimeError("Could not load " + map_path)
        surface = world_surface()
        surface.set_editor_property("detail_material", material)
        if not unreal.EditorLevelLibrary.save_current_level():
            raise RuntimeError("Could not save " + map_path)
        del surface
        gc.collect()
        if not unreal.EditorLevelLibrary.load_level(map_path):
            raise RuntimeError("Could not reload " + map_path)
        saved = world_surface()
        near = saved.get_editor_property("detail_material")
        far = saved.get_editor_property("surface_material")
        if near is None or near.get_path_name().split(".")[0] != MATERIAL:
            raise RuntimeError("Near PBR material did not survive save in " + map_path)
        if far is not None and far.get_path_name().split(".")[0] == MATERIAL:
            raise RuntimeError("Far globe accidentally uses detailed PBR in " + map_path)
        results[map_path] = {"near_patch": near.get_path_name(),
                             "far_globe": far.get_path_name() if far else "M_WorldVertex fallback",
                             "saved_and_reloaded": True}
        del saved, near, far
        gc.collect()
    REPORT.write_text(json.dumps(results, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_TERRAIN_PBR_ASSIGNED " + str(len(results)))


main()
