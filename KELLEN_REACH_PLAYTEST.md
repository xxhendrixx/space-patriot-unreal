# Kellen Reach in-Editor playtest

Open `SpacePatriotUnreal.uproject` in Unreal Engine 5.8. The on-foot test map is
`/Game/SpacePatriot/Maps/L_KellenReachWalk`. Press **Play** in the Editor toolbar
and choose **Selected Viewport** (not Standalone Game) if Unreal asks for a play
mode. This keeps the test inside the Editor.

The map reuses the Kellen Reach flight-port layout but gives it a walkable
concrete apron, raised runway marks, cargo bins and three CC0 rock meshes. Epic's
First Person Arena Shooter example supplies the placeholder character, rifle,
enemy, pickup, AI and crosshair/score/ammo HUD. Walk with **WASD**, look with the
mouse, and fire with **left click**. Walk over the pickup near the spawn to equip
the rifle. The `SP Walk / rifle pickup` actor is explicitly configured with the
`DT_WeaponList` `Rifle` row; it is not the template's default Pistol pickup.
An AI patrol and spawner sit farther along the apron. A NavMeshBoundsVolume
covers the test route. The world runtime actor still receives Player 0 input:
**F5** starts the original Storyworks `water` quest and **F6** saves society,
campaign and survey data. The walk map's GameMode overrides the project default
with Epic's shooter character and controller.

To test flight instead, open `/Game/SpacePatriot/Maps/L_KestrelFlight` and press
**Play** in Selected Viewport. **W/S** move forward/back, **A/D** strafe,
**Space/Ctrl** rise/descend, arrow keys pitch/yaw, **Q/E** roll, **X** brake,
**Shift** boost, **G** gear, **L** land, **V** change camera. **F1/F2** select
engineering presets; **F3/F4** cycle/dim the MFD; **Tab** gives the MFD buttons
a mouse pointer. The world runtime uses the same **F5/F6** quest/save keys.

`Data/KellenReachWalkValidation.json` records 38 passing fresh-Editor-load
checks for the shooter GameMode/pawn/controller, saved placement, configured
Rifle pickup, AI controller, nav bounds, HUD dependency and world runtime.
`Data/KellenReachNavigationValidation.json` records seven passing fresh-load
checks, including complete paths from spawn to the NPC and from the NPC to the
spawner. Press **P** in the Editor to inspect the green navigation overlay.
`Data/KestrelFlightInputValidation.json` records four passing flight-map checks.
These are saved-map and path-query checks. They do **not** prove live controls, enemy movement,
damage, pickup overlap, frame rate or HUD readability in PIE. Please report
those observations from Selected Viewport; the next integration pass should
connect Epic's temporary shooter loop to Space Patriot's native combat and
inventory simulations, then replace the placeholder art.

The imported character, rifle, AI and HUD are temporary Epic template assets.
Run `Tools/InstallLocalDependencies.ps1` after cloning or updating the project
before opening either playtest map. The stock Arena sample map is not installed;
the dependency manifest stages only the assets used by Space Patriot.
