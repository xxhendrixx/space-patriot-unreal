"""Duplicate the dressed Kestrel map without reloading it in the same process.

UE 5.8 keeps a standalone UWorld reference after duplicate_asset, so the
new map must be opened by a fresh Editor process for further editing.
"""

import unreal


source = "/Game/SpacePatriot/Maps/L_KestrelFlight"
target = "/Game/SpacePatriot/Maps/L_KellenReachWalk"
if not unreal.EditorAssetLibrary.does_asset_exist(source):
    raise RuntimeError("Missing dressed source map: " + source)
if not unreal.EditorAssetLibrary.does_asset_exist(target):
    duplicate = unreal.EditorAssetLibrary.duplicate_asset(source, target)
    if duplicate is None:
        raise RuntimeError("Could not duplicate Kestrel map")
    if not unreal.EditorAssetLibrary.save_asset(target, only_if_is_dirty=False):
        raise RuntimeError("Could not save duplicated walk map")
unreal.log("SPACE_PATRIOT_WALK_MAP_DUPLICATED " + target)
