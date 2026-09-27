"""Run inside Blender with the approved Kestrel .blend open.

The Unity assembly is X-forward/Y-up/Z-side. Unreal is X-forward/Z-up/Y-side.
Rotate every mesh +90 degrees around X before FBX export. Retain all three
mesh LODs and each independent part so landing gear remains separately usable.

blender -b Kestrel_K017.blend --python Tools/ConvertKestrelForUnreal.py -- OUTPUT.fbx
"""

import bpy
from math import pi
from mathutils import Matrix
from pathlib import Path
import sys


if "--" not in sys.argv:
    raise RuntimeError("Pass an output FBX path after --")
output = Path(sys.argv[sys.argv.index("--") + 1]).resolve()
output.parent.mkdir(parents=True, exist_ok=True)
meshes = [obj for obj in bpy.data.objects if obj.type == "MESH"]
expected = {part + "_LOD" + str(lod) for part in (
    "Hull", "Wing_Port", "Wing_Starboard", "Drive_Port", "Drive_Starboard",
    "Gear_Nose", "Gear_Port", "Gear_Starboard",
) for lod in range(3)}
if {obj.name for obj in meshes} != expected:
    raise RuntimeError("Expected exactly the eight Kestrel parts at three LODs")

conversion = Matrix.Rotation(pi / 2, 4, "X")
for obj in meshes:
    obj.matrix_world = conversion @ obj.matrix_world

bpy.ops.object.select_all(action="DESELECT")
for obj in meshes:
    obj.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.export_scene.fbx(
    filepath=str(output),
    use_selection=True,
    object_types={"MESH"},
    add_leaf_bones=False,
    axis_forward="-Z",
    axis_up="Y",
    apply_unit_scale=True,
    path_mode="COPY",
    embed_textures=False,
)
print("Exported Unreal X-forward/Z-up Kestrel:", output)
