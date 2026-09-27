"""Create a separate triplanar PBR material for the Worldworks surface.

Run after ImportPolyHaven.py has imported the three terrain texture sets.
This creates only M_WorldTerrainPBR; it never edits M_WorldVertex or a map.
Assign it to SPWorldSurface.DetailMaterial only after a visual/performance pass.
"""

import json
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
DIRECTORY = "/Game/SpacePatriot/Materials"
MATERIAL = DIRECTORY + "/M_WorldTerrainPBR"
FALLBACK = DIRECTORY + "/M_WorldVertex"
REPORT = PROJECT / "Validation" / "world-terrain-material-build.json"
FUNCTIONS = {
    "color": "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture",
    "normal": "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal",
}
BASE = "/Game/SpacePatriot/OpenAssets/PolyHaven/Terrain"
SETS = {
    "rock": {
        "folder": "RocksGround02", "stem": "rocks_ground_02", "tile_cm": 200.0,
    },
    "grass": {
        "folder": "GrassGround", "stem": "grass_ground", "tile_cm": 251.0,
    },
    "sand": {
        "folder": "RedSand", "stem": "red_sand", "tile_cm": 300.0,
    },
}
SUFFIX = {
    "albedo": "diff_2k", "normal": "nor_dx_1k",
    "roughness": "rough_1k", "ao": "ao_1k",
}
MEL = unreal.MaterialEditingLibrary


def asset(path, expected_type):
    result = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(result, expected_type):
        raise RuntimeError("Missing or wrong asset type: " + path)
    return result


def tex_path(layer, channel):
    spec = SETS[layer]
    return f"{BASE}/{spec['folder']}/{spec['stem']}_{SUFFIX[channel]}"


def new(material, cls, x, y, description=None):
    node = MEL.create_material_expression(material, cls, x, y)
    if node is None:
        raise RuntimeError("Could not create material expression: " + str(cls))
    if description:
        node.set_editor_property("desc", description)
    return node


def link(source, output, target, input_name):
    inputs = list(MEL.get_material_expression_input_names(target))
    # Several one-input expression classes expose their pin as "None" through
    # the Python reflection API; Unreal connects it using an empty pin name.
    resolved_input = "" if len(inputs) == 1 and inputs[0] in ("", "None") else input_name
    if not MEL.connect_material_expressions(source, output, target, resolved_input):
        raise RuntimeError(
            f"Could not link {source.get_name()}.{output} to {target.get_name()}.{resolved_input}; "
            f"source outputs={list(MEL.get_material_expression_output_names(source))} "
            f"target inputs={inputs}"
        )


def output(material, node, name, property_):
    if not MEL.connect_material_property(node, name, property_):
        raise RuntimeError("Could not connect material property " + str(property_))


def scalar(material, value, x, y, description=None):
    node = new(material, unreal.MaterialExpressionConstant, x, y, description)
    node.set_editor_property("r", float(value))
    return node


def unary(material, cls, source, x, y, description=None, input_name="Input"):
    node = new(material, cls, x, y, description)
    link(source, "", node, input_name)
    return node


def binary(material, cls, a, b, x, y, description=None, a_output="", b_output=""):
    node = new(material, cls, x, y, description)
    link(a, a_output, node, "A")
    link(b, b_output, node, "B")
    return node


def lerp(material, a, b, alpha, x, y, description=None):
    node = new(material, unreal.MaterialExpressionLinearInterpolate, x, y, description)
    link(a, "", node, "A")
    link(b, "", node, "B")
    link(alpha, "", node, "Alpha")
    return node


def channel(material, source, red, green, blue, x, y, description):
    node = new(material, unreal.MaterialExpressionComponentMask, x, y, description)
    node.set_editor_properties({"r": red, "g": green, "b": blue, "a": False})
    link(source, "", node, "")
    return node


def aligned_sample(material, texture, size_node, function, x, y, description, scalar_result):
    tex = new(material, unreal.MaterialExpressionTextureObject, x, y, description + " source")
    tex.set_editor_property("texture", texture)
    call = new(material, unreal.MaterialExpressionMaterialFunctionCall, x + 260, y, description)
    if not call.set_material_function(function):
        raise RuntimeError("Could not set material function on " + description)
    input_names = set(MEL.get_material_expression_input_names(call))
    output_names = set(MEL.get_material_expression_output_names(call))
    if not {"TextureObject", "TextureSize"}.issubset(input_names) or "XYZ Texture" not in output_names:
        raise RuntimeError(
            f"Unexpected WorldAligned function interface for {description}: "
            f"inputs={sorted(input_names)} outputs={sorted(output_names)}"
        )
    link(tex, "", call, "TextureObject")
    link(size_node, "", call, "TextureSize")
    # Explicitly select XYZ rather than letting a multi-output function call
    # fall back to its first (usually XY) projection and reintroduce seams.
    mask = new(material, unreal.MaterialExpressionComponentMask, x + 520, y,
               description + (" red channel" if scalar_result else " XYZ color"))
    mask.set_editor_properties({"r": True, "g": not scalar_result,
                                "b": not scalar_result, "a": False})
    link(call, "XYZ Texture", mask, "")
    return mask


def biome_weights(material, vertex):
    red = channel(material, vertex, True, False, False, -1200, 2800, "source biome red")
    green = channel(material, vertex, False, True, False, -1200, 2960, "source biome green")
    blue = channel(material, vertex, False, False, True, -1200, 3120, "source biome blue")

    # Source SurfaceColor is greener on vegetated temperate ground and redder
    # on arid ground. The blue test prevents icy worlds from becoming grass.
    green_red = binary(material, unreal.MaterialExpressionSubtract, green, red, -920, 2800)
    grass_bias = binary(material, unreal.MaterialExpressionAdd, green_red,
                        scalar(material, 0.04, -1110, 3400), -680, 2800)
    grass_a = unary(material, unreal.MaterialExpressionSaturate,
                    binary(material, unreal.MaterialExpressionMultiply, grass_bias,
                           scalar(material, 8.0, -900, 3420), -440, 2800), -220, 2800)
    green_blue = binary(material, unreal.MaterialExpressionSubtract, green, blue, -920, 3100)
    grass_b = unary(material, unreal.MaterialExpressionSaturate,
                    binary(material, unreal.MaterialExpressionMultiply, green_blue,
                           scalar(material, 10.0, -900, 3520), -440, 3100), -220, 3100)
    grass_weight = binary(material, unreal.MaterialExpressionMultiply,
                          grass_a, grass_b, 20, 2920, "source-derived grass weight")

    red_green = binary(material, unreal.MaterialExpressionSubtract, red, green, -920, 3720)
    sand_delta = binary(material, unreal.MaterialExpressionSubtract, red_green,
                        scalar(material, 0.04, -900, 3920), -680, 3720)
    sand_a = unary(material, unreal.MaterialExpressionSaturate,
                   binary(material, unreal.MaterialExpressionMultiply, sand_delta,
                          scalar(material, 8.0, -900, 4100), -440, 3720), -220, 3720)
    sand_absolute = binary(material, unreal.MaterialExpressionSubtract, red,
                           scalar(material, 0.36, -900, 4350), -680, 4200)
    sand_b = unary(material, unreal.MaterialExpressionSaturate,
                   binary(material, unreal.MaterialExpressionMultiply, sand_absolute,
                          scalar(material, 12.0, -900, 4500), -440, 4200), -220, 4200)
    sand_weight = binary(material, unreal.MaterialExpressionMultiply,
                         sand_a, sand_b, 20, 3960, "source-derived arid weight")
    return grass_weight, sand_weight


def build():
    if not unreal.EditorAssetLibrary.does_asset_exist(FALLBACK):
        raise RuntimeError("Missing untouched fallback material " + FALLBACK)
    functions = {key: asset(path, unreal.MaterialFunctionInterface)
                 for key, path in FUNCTIONS.items()}
    textures = {}
    for layer in SETS:
        textures[layer] = {}
        for channel in SUFFIX:
            texture = asset(tex_path(layer, channel), unreal.Texture2D)
            is_color = channel == "albedo"
            if bool(texture.get_editor_property("srgb")) != is_color:
                raise RuntimeError("Wrong sRGB setting: " + tex_path(layer, channel))
            compression = str(texture.get_editor_property("compression_settings")).upper()
            if channel == "normal" and "NORMALMAP" not in compression:
                raise RuntimeError("Normal is not TC_NORMALMAP: " + tex_path(layer, channel))
            if channel in ("roughness", "ao") and "MASKS" not in compression:
                raise RuntimeError("Mask is not TC_MASKS: " + tex_path(layer, channel))
            textures[layer][channel] = texture

    # Never replace an artist-edited material. Build once and use the separate
    # validator to inspect the saved package on later runs.
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL):
        raise RuntimeError(MATERIAL + " already exists; inspect it before any rebuild")
    unreal.EditorAssetLibrary.make_directory(DIRECTORY)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_WorldTerrainPBR", DIRECTORY, unreal.Material, unreal.MaterialFactoryNew()
    )
    if material is None:
        raise RuntimeError("Could not create " + MATERIAL)
    # WorldAlignedNormal's XYZ projection is in world space; the material's
    # Normal input must interpret it in that space, especially on steep slopes.
    material.set_editor_property("tangent_space_normal", False)

    vertex = new(material, unreal.MaterialExpressionVertexColor, -1540, 2820,
                 "original Worldworks biome palette; no mesh UV dependence")
    grass_weight, sand_weight = biome_weights(material, vertex)
    layers = {}
    for index, (layer, spec) in enumerate(SETS.items()):
        size = new(material, unreal.MaterialExpressionConstant3Vector,
                   -1900, index * 1450 + 120, layer + " triplanar tile size, cm")
        size.set_editor_property("constant", unreal.LinearColor(
            spec["tile_cm"], spec["tile_cm"], spec["tile_cm"], 1.0
        ))
        layers[layer] = {}
        for channel_index, channel in enumerate(SUFFIX):
            function = functions["normal" if channel == "normal" else "color"]
            layers[layer][channel] = aligned_sample(
                material, textures[layer][channel], size, function,
                -1680, index * 1450 + channel_index * 310,
                f"{layer} {channel} world-aligned XYZ", channel in ("roughness", "ao")
            )

    result = {}
    for channel, y in (("albedo", 0), ("normal", 600),
                       ("roughness", 1200), ("ao", 1800)):
        grass_mix = lerp(material, layers["rock"][channel], layers["grass"][channel],
                         grass_weight, 400, y, channel + " rock/grass blend")
        result[channel] = lerp(material, grass_mix, layers["sand"][channel],
                               sand_weight, 720, y, channel + " arid blend")

    # The original vertex palette remains visible without washing out the
    # licensed albedo. All three texture layers retain their own PBR maps.
    tint = scalar(material, 0.30, 780, 325, "source palette blend")
    base = new(material, unreal.MaterialExpressionLinearInterpolate,
               1050, 0, "terrain texture with source biome color")
    link(result["albedo"], "", base, "A")
    link(vertex, "", base, "B")
    link(tint, "", base, "Alpha")
    normal = unary(material, unreal.MaterialExpressionNormalize,
                   result["normal"], 1050, 600, "normalize blended triplanar normals",
                   input_name="VectorInput")
    output(material, base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    output(material, normal, "", unreal.MaterialProperty.MP_NORMAL)
    output(material, result["roughness"], "", unreal.MaterialProperty.MP_ROUGHNESS)
    output(material, result["ao"], "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Could not save " + MATERIAL)
    report = {
        "material": MATERIAL,
        "fallback_untouched": FALLBACK,
        "projection": "WorldAlignedTexture/WorldAlignedNormal XYZ triplanar, world centimetres",
        "normal_space": "world",
        "uses_mesh_uv": False,
        "surface_map_modified": False,
        "texture_sets": {
            layer: {"tile_cm": spec["tile_cm"],
                    "textures": {channel: tex_path(layer, channel) for channel in SUFFIX}}
            for layer, spec in SETS.items()
        },
        "expression_count": MEL.get_num_material_expressions(material),
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_PATRIOT_WORLD_TERRAIN_PBR_BUILT " + json.dumps(report))


build()
