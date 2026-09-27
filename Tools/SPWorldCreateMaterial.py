"""Create the lit vertex-color material for ASPWorldSurface in Unreal Editor.

Run with UnrealEditor-Cmd -ExecutePythonScript after compiling the native module.
The asset is committed after generation; players do not need to run this tool.
"""

import unreal

DIRECTORY = "/Game/SpacePatriot/Materials"
NAME = "M_WorldVertex"
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
        color = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVertexColor, -450, 0
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
        )
        roughness = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionConstant, -250, 200
        )
        roughness.set_editor_property("r", 0.86)
        unreal.MaterialEditingLibrary.connect_material_property(
            roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
        )
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log("SPACE_PATRIOT_WORLD_MATERIAL " + PATH)


main()
