# Space Patriot — Unreal Engine 5.8 project

This is the Unreal development branch of Space Patriot. The first map now contains the accepted symmetric Kestrel K-017 mesh, a Blueprint pawn for editor flight, a Kellen Reach landing apron, imported materials, and the 19-world/190-creature source catalogs. It is an early playable slice, not a finished transfer of the Unity game.

## Open and play

1. Install Git LFS, Unreal Engine 5.8, Visual Studio 2022 C++ Build Tools, Windows SDK 10.0.22621 or newer, and the .NET Framework SDK. Run `git lfs install` once, clone this repository, and run `git lfs pull` inside the clone.
2. Open **`SpacePatriotUnreal.uproject`**. Accept Unreal's request to compile the `SpacePatriotUnreal` module. The project uses the UE 5.8 V7 build settings.
3. The startup map is `/Game/SpacePatriot/Maps/L_KestrelFlight`. Press **Play**. `BP_KestrelGameMode` spawns `BP_KestrelFlyable`, which carries eight Kestrel parts and uses Unreal's `DefaultPawn` movement.

The Kestrel pawn is a flight prototype. Its camera and collision are still temporary, and the port has not yet reproduced Unity's throttle, atmospheric flight, landing gear articulation, cockpit displays, weapons, cargo handling, settlements, terrain streaming, animation, sound, or quests. The broader universe C++ simulation and wildlife catalog are in this repository but need integration and visual work in the main map.

## Project contents

- `Content/SpacePatriot/Maps/L_KestrelFlight.umap`: initial Kellen Reach flight map.
- `Content/SpacePatriot/Blueprints/BP_KestrelFlyable.uasset`: free-flight player pawn with the Kestrel mesh.
- `Content/SpacePatriot/Ships/KestrelK017/`: imported ship meshes and textures. The mesh was converted from X-forward/Y-up Unity art into Unreal's X-forward/Z-up coordinates; the symmetric wing and drive pairs occupy matching ±Y positions.
- `SourceAssets/KestrelK017/`: the reimportable, Unreal-oriented FBX and texture source. The original Blender project remains in the Unity repository.
- `Source/SpacePatriotUnreal/`: Blueprint-facing native components and actor bases for worlds, wildlife, NPC decisions, cargo, story beats, and MFD interaction.
- `Data/Worlds.json` and `Data/CreatureRosters.json`: stable world and wildlife identities.
- `Tools/ImportKestrel.py`, `Tools/BuildPlayablePrototype.py`, `Tools/ValidatePlayablePrototype.py`: reproducible Editor import, map creation, and validation. Assets in `Content/` are committed, so opening the map does **not** require rerunning these scripts.
- `Tools/CreateBlueprintHandoff.py`: creates optional Blueprint subclasses from the native systems and a separate systems test map.

`Data/PlayableValidation.json` records automated Editor checks. See `PORT_MAP.md` for the remaining subsystem work. The Kestrel's geometry still has visible bake seams and imperfect gear mounts; importing it into Unreal does not approve those art defects.

## Working with a friend

After cloning once, run `git pull` followed by `git lfs pull` to receive incremental changes. Unreal-generated `Binaries`, `DerivedDataCache`, `Intermediate`, and `Saved` folders stay local. Commit source changes plus deliberate `Content/` assets; Git LFS tracks `.uasset`, `.umap`, `.fbx`, and `.png` files.
