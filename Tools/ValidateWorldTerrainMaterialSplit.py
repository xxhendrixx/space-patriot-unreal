"""Exercise far/near material selection without saving or changing a map."""

import json
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
REPORT = PROJECT / "Validation" / "world-terrain-material-split.json"
FAR = "/Game/SpacePatriot/Materials/M_WorldVertex"
NEAR = "/Game/SpacePatriot/Materials/M_WorldTerrainPBR"


def main():
    far = unreal.EditorAssetLibrary.load_asset(FAR)
    near = unreal.EditorAssetLibrary.load_asset(NEAR)
    if not isinstance(far, unreal.MaterialInterface) or not isinstance(near, unreal.MaterialInterface):
        raise RuntimeError("Build both far and near terrain materials first")
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SPWorldSurface, unreal.Vector(1000000, 1000000, 1000000)
    )
    if actor is None:
        raise RuntimeError("Could not spawn temporary SPWorldSurface")
    checks = []

    def check(name, ok, detail):
        checks.append({"name": name, "passed": bool(ok), "detail": str(detail)})
        if not ok:
            unreal.log_error("WORLD_TERRAIN_SPLIT_FAIL " + name + " " + str(detail))

    try:
        actor.set_editor_property("world_id", "earth")
        actor.set_editor_property("surface_material", far)
        actor.set_editor_property("detail_material", near)
        check("rebuild_with_near_material", actor.rebuild_surface(), actor.get_actor_label())
        planet = actor.get_editor_property("planet_mesh")
        detail = actor.get_editor_property("detail_mesh")
        check("globe_keeps_vertex_fallback", planet.get_material(0) == far,
              planet.get_material(0))
        check("streamed_patch_uses_pbr", detail.get_material(0) == near,
              detail.get_material(0))

        actor.set_editor_property("detail_material", None)
        check("rebuild_without_override", actor.rebuild_surface(), actor.get_actor_label())
        check("null_override_reuses_fallback", detail.get_material(0) == far,
              detail.get_material(0))
    finally:
        unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)

    result = {"far_material": FAR, "near_material": NEAR, "saved_map_modified": False,
              "passed": sum(x["passed"] for x in checks),
              "failed": sum(not x["passed"] for x in checks), "checks": checks}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
    if result["failed"]:
        raise RuntimeError("Material split validation failed; see " + str(REPORT))
    unreal.log("SPACE_PATRIOT_WORLD_TERRAIN_MATERIAL_SPLIT " + json.dumps(result))


main()
