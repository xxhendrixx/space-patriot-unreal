# Space Patriot — Unreal Engine 5.8 project

This is the Unreal development branch of Space Patriot. The first map contains the symmetric Kestrel K-017 mesh, a native flight pawn, the Kellen Reach apron, a bounded Earth terrain surface, and runtime components for society, the Long Debt campaign and field surveying. It is an early playable slice, not a finished transfer of the Unity or original browser game.

## Open and play

1. Install Git LFS, Unreal Engine 5.8, Visual Studio 2022 C++ Build Tools, Windows SDK 10.0.22621 or newer, and the .NET Framework SDK. Run `git lfs install` once, clone this repository, and run `git lfs pull` inside the clone.
2. Open **`SpacePatriotUnreal.uproject`**. Accept Unreal's request to compile the `SpacePatriotUnreal` module. The project uses the UE 5.8 V7 build settings.
3. The startup map is `/Game/SpacePatriot/Maps/L_KestrelFlight`. Press **Play**. `BP_KestrelGameMode` spawns `BP_KestrelFlyable`, which carries eight Kestrel parts and derives from native `ASPFlightPawn`. The grounded display ship is visible in the Editor and hidden during Play.

The mapped flight keys are W/S forward/back, A/D strafe, Space/Ctrl rise/descent, arrows pitch/yaw, Q/E roll, X brake, Shift boost, G gear, T assist, B cruise, P power, V camera and L request landing. W movement was visually checked in a live game window; the other keys have native API/automation coverage but still need a full hands-on control pass. The pawn camera/collision and ship materials are provisional: the current exterior still looks washed out, and cockpit displays, vessel engineering, cargo interaction, visible citizens and settlements, encounters, sound, travel, and quest UI remain incomplete. See `PORT_AUDIT.md` for the specific gaps and validation evidence.

## Project contents

- `Content/SpacePatriot/Blueprints/BP_KestrelFlyable.uasset`: native-flight player pawn with the eight Kestrel meshes.
- `Content/SpacePatriot/Maps/L_KestrelFlight.umap`: apron, one bounded Earth `ASPWorldSurface`, and `BP_WorldRuntime` with society, Long Debt and Field Survey components.
- `Content/SpacePatriot/Ships/KestrelK017/`: imported ship meshes and textures. The mesh was converted from X-forward/Y-up Unity art into Unreal's X-forward/Z-up coordinates; the symmetric wing and drive pairs occupy matching ±Y positions.
- `SourceAssets/KestrelK017/`: the reimportable, Unreal-oriented FBX and texture source. The original Blender project remains in the Unity repository.
- `Source/SpacePatriotUnreal/`: native flight, bounded Worldworks terrain, society/cargo, Long Debt graph, Field Survey and tested architecture layout/route components, plus earlier Blueprint bases.
- `Data/Worlds.json`, `Data/CreatureRosters.json`, `Data/Worldworks/`, `Data/Settlements.json`, `Data/StoryCampaign.json`, and `Data/Architecture/DeckPlans.json`: source world, wildlife, terrain/climate, settlement, campaign and interior-layout data.
- `Tools/ImportKestrel.py`, `Tools/BuildPlayablePrototype.py`, `Tools/IntegratePlayableSystems.py`, and `Tools/SPWorldIntegrate.py`: reproducible Editor import, map construction and integration. Assets in `Content/` are committed, so opening the map does **not** require rerunning these scripts.
- `Tools/CreateBlueprintHandoff.py`: creates optional Blueprint subclasses from the native systems and a separate systems test map.

`Data/PlayableValidation.json`, `Data/PlayableSystemsIntegration.json`, and `Data/WorldRuntimeValidation.json` record Editor checks. `Validation/kestrel-native-flight-play.png` is a real game screenshot. The Kestrel's geometry still has visible bake seams and imperfect gear mounts; importing it into Unreal does not approve those art defects.

## Working with a friend

After cloning once, run `git pull` followed by `git lfs pull` to receive incremental changes. Unreal-generated `Binaries`, `DerivedDataCache`, `Intermediate`, and `Saved` folders stay local. Commit source changes plus deliberate `Content/` assets; Git LFS tracks `.uasset`, `.umap`, `.fbx`, `.png`, and Worldworks `.bytes` files.
