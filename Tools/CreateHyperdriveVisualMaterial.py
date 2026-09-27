"""Create the cooked, unlit vertex-color material for the procedural travel FX.

Run once with UnrealEditor-Cmd -ExecutePythonScript=Tools/CreateHyperdriveVisualMaterial.py.
The saved .uasset is committed; players do not need Python or Niagara.
"""

import unreal

DIRECTORY = "/Game/SpacePatriot/Materials"
NAME = "M_HyperdriveVisual"
PATH = DIRECTORY + "/" + NAME


def main():
    unreal.EditorAssetLibrary.make_directory(DIRECTORY)
    material = unreal.EditorAssetLibrary.load_asset(PATH)
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            NAME, DIRECTORY, unreal.Material, unreal.MaterialFactoryNew()
        )
        if material is None:
            raise RuntimeError("Unable to create " + PATH)
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property("two_sided", True)

        vertex = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVertexColor, -450, 0
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            vertex, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            vertex, "A", unreal.MaterialProperty.MP_OPACITY
        )
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log("SPACE_PATRIOT_HYPERDRIVE_MATERIAL " + PATH)


main()
