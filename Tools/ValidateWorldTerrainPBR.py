"""Read-only saved-asset audit of the Worldworks triplanar PBR material.

This verifies the material graph and imported textures. It does not assign a
level material, compile a packaged build, or claim visual/performance parity.
"""

import json
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MATERIAL = "/Game/SpacePatriot/Materials/M_WorldTerrainPBR"
FALLBACK = "/Game/SpacePatriot/Materials/M_WorldVertex"
REPORT = PROJECT / "Validation" / "world-terrain-material.json"
BASE = "/Game/SpacePatriot/OpenAssets/PolyHaven/Terrain"
TEXTURES = {
    layer: [f"{BASE}/{folder}/{stem}_{suffix}" for suffix in
            ("diff_2k", "nor_dx_1k", "rough_1k", "ao_1k")]
    for layer, folder, stem in (
        ("rock", "RocksGround02", "rocks_ground_02"),
        ("grass", "GrassGround", "grass_ground"),
        ("sand", "RedSand", "red_sand"),
    )
}
FUNCTIONS = {
    "color": "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture",
    "normal": "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal",
}


def validate():
    mel = unreal.MaterialEditingLibrary
    failures = []
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL)
    fallback = unreal.EditorAssetLibrary.load_asset(FALLBACK)
    if not isinstance(material, unreal.Material):
        failures.append("Missing M_WorldTerrainPBR")
    if not isinstance(fallback, unreal.Material):
        failures.append("Missing untouched M_WorldVertex fallback")
    observed = {"material": MATERIAL, "fallback": FALLBACK, "textures": {},
                "material_functions": {}, "property_inputs": {}, "failures": failures}
    for layer, paths in TEXTURES.items():
        observed["textures"][layer] = []
        for index, path in enumerate(paths):
            texture = unreal.EditorAssetLibrary.load_asset(path)
            if not isinstance(texture, unreal.Texture2D):
                failures.append("Missing Texture2D " + path)
                continue
            srgb = bool(texture.get_editor_property("srgb"))
            compression = str(texture.get_editor_property("compression_settings"))
            observed["textures"][layer].append({"asset": path, "srgb": srgb,
                                                   "compression": compression})
            if srgb != (index == 0):
                failures.append("Wrong sRGB setting " + path)
            if index == 1 and "NORMALMAP" not in compression.upper():
                failures.append("Normal texture not TC_NORMALMAP " + path)
            if index in (2, 3) and "MASKS" not in compression.upper():
                failures.append("Roughness/AO texture not TC_MASKS " + path)

    if isinstance(material, unreal.Material):
        tangent_space_normal = bool(material.get_editor_property("tangent_space_normal"))
        observed["tangent_space_normal"] = tangent_space_normal
        if tangent_space_normal:
            failures.append("WorldAlignedNormal output requires world-space material Normal input")
        expressions = list(mel.get_material_expressions(material))
        texture_nodes = [x for x in expressions if isinstance(x, unreal.MaterialExpressionTextureObject)]
        function_nodes = [x for x in expressions if isinstance(x, unreal.MaterialExpressionMaterialFunctionCall)]
        vertex_nodes = [x for x in expressions if isinstance(x, unreal.MaterialExpressionVertexColor)]
        observed["expression_count"] = len(expressions)
        observed["texture_object_count"] = len(texture_nodes)
        observed["function_call_count"] = len(function_nodes)
        observed["vertex_color_count"] = len(vertex_nodes)
        used_textures = {
            x.get_editor_property("texture").get_path_name().split(".")[0]
            for x in texture_nodes if x.get_editor_property("texture")
        }
        required_textures = {path for paths in TEXTURES.values() for path in paths}
        if used_textures != required_textures:
            failures.append("Material texture references differ from required terrain sets")
        if len(texture_nodes) != 12 or len(function_nodes) != 12 or len(vertex_nodes) != 1:
            failures.append("Expected twelve projected texture/function pairs and one vertex palette")
        used_functions = {}
        for node in function_nodes:
            function = node.get_editor_property("material_function")
            path = function.get_path_name().split(".")[0] if function else "missing"
            used_functions[path] = used_functions.get(path, 0) + 1
        observed["material_functions"] = used_functions
        if used_functions.get(FUNCTIONS["color"]) != 9 or used_functions.get(FUNCTIONS["normal"]) != 3:
            failures.append("World-aligned material function count differs from 9 color / 3 normal")
        for name, property_ in (
            ("base_color", unreal.MaterialProperty.MP_BASE_COLOR),
            ("normal", unreal.MaterialProperty.MP_NORMAL),
            ("roughness", unreal.MaterialProperty.MP_ROUGHNESS),
            ("ambient_occlusion", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),
        ):
            node = mel.get_material_property_input_node(material, property_)
            observed["property_inputs"][name] = node.get_class().get_name() if node else None
            if node is None:
                failures.append("Unconnected material property " + name)
    observed["ok"] = not failures
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(observed, indent=2), encoding="utf-8")
    if failures:
        raise RuntimeError("World terrain PBR validation failed: " + "; ".join(failures))
    unreal.log("SPACE_PATRIOT_WORLD_TERRAIN_PBR_VALID " + json.dumps({
        "expressions": observed["expression_count"],
        "textures": observed["texture_object_count"],
        "functions": observed["function_call_count"],
        "fallback": FALLBACK,
    }))


validate()
