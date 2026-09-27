"""Read-only post-import checks for the three Poly Haven CC0 packs."""

from pathlib import Path
import json
import traceback
import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE_REPORT = PROJECT / "Data" / "PolyHavenImportReport.json"
REPORT = PROJECT / "Data" / "PolyHavenValidationReport.json"


def xyz(vec):
    return [float(vec.x), float(vec.y), float(vec.z)]


def main():
    source = json.loads(SOURCE_REPORT.read_text(encoding="utf-8"))
    checks = {"textures": {}, "meshes": {}, "materials": {}}
    failures = []

    for key, paths in source["imported"].items():
        if key.endswith("_meshes"):
            continue
        texture = unreal.EditorAssetLibrary.load_asset(paths[0])
        if not isinstance(texture, unreal.Texture2D):
            failures.append("Not Texture2D: " + paths[0])
            continue
        srgb = bool(texture.get_editor_property("srgb"))
        compression = str(texture.get_editor_property("compression_settings"))
        checks["textures"][key] = {
            "asset": paths[0], "srgb": srgb, "compression": compression
        }
        if "normal" in key and (srgb or "NORMALMAP" not in compression.upper()):
            failures.append("Normal texture settings incorrect: " + key)
        if any(c in key for c in ("roughness", "ao", "opacity")) and srgb:
            failures.append("Non-color texture still sRGB: " + key)

    for key in ("boulder_meshes", "crate_meshes"):
        for path in source["imported"][key]:
            mesh = unreal.EditorAssetLibrary.load_asset(path)
            if not isinstance(mesh, unreal.StaticMesh):
                failures.append("Not StaticMesh: " + path)
                continue
            box = mesh.get_bounding_box()
            extent_cm = [
                float(box.max.x - box.min.x),
                float(box.max.y - box.min.y),
                float(box.max.z - box.min.z),
            ]
            material = mesh.get_material(0)
            material_path = material.get_path_name() if material else None
            checks["meshes"][path] = {
                "bounds_min_cm": xyz(box.min),
                "bounds_max_cm": xyz(box.max),
                "size_cm": extent_cm,
                "num_lods": mesh.get_num_lods(),
                "triangles_lod0": mesh.get_num_triangles(0),
                "material_slot_0": material_path,
            }
            desired = source["boulder_material"] if key == "boulder_meshes" else source["crate_material"]
            if not material_path or desired not in material_path:
                failures.append("Wrong mesh material: " + path)
            if max(extent_cm) < 25 or max(extent_cm) > 500:
                failures.append("Likely bad import scale: " + path + " " + str(extent_cm))

    for key in ("concrete_material", "boulder_material", "crate_material"):
        path = source[key]
        material = unreal.EditorAssetLibrary.load_asset(path)
        checks["materials"][key] = {
            "asset": path,
            "exists": isinstance(material, unreal.Material),
            "blend_mode": str(material.get_editor_property("blend_mode")) if material else None,
        }
        if not isinstance(material, unreal.Material):
            failures.append("Missing material: " + path)

    result = {"checks": checks, "failures": failures, "ok": not failures}
    REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
    if failures:
        raise RuntimeError("Poly Haven validation failed: " + "; ".join(failures))
    unreal.log("SPACE_PATRIOT_POLYHAVEN_VALIDATION_OK")


try:
    main()
except Exception:
    REPORT.write_text(traceback.format_exc(), encoding="utf-8")
    raise
