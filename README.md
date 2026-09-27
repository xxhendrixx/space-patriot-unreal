# Space Patriot — Unreal Engine 5.8 project

This is the Unreal development branch of Space Patriot. Two Kellen Reach maps now share the port, world surface and runtime components for society, the Long Debt campaign and field surveying. One map tests the Kestrel flight pawn; the other tests on-foot movement, an Epic example rifle pickup, NPC/AI and shooter HUD. These are mechanics placeholders and an early playable slice, not a finished transfer of the Unity or original browser game.

## Open and play

1. Install Git LFS, Unreal Engine 5.8, Visual Studio 2022 C++ Build Tools, Windows SDK 10.0.22621 or newer, and the .NET Framework SDK. Run `git lfs install` once, clone this repository, and run `git lfs pull` inside the clone.
2. With the Editor closed, run `./Tools/InstallLocalDependencies.ps1` in PowerShell. It copies the exact required Epic First Person template assets from the installed engine, downloads the listed Poly Haven CC0 files, builds the project Editor module on a fresh clone, and imports their Unreal assets. The downloadable packs are intentionally absent from Git; `-List` shows the manifest and `-VerifyOnly` checks an existing installation. The first import takes several minutes.
3. Open **`SpacePatriotUnreal.uproject`**. Accept Unreal's request to compile the `SpacePatriotUnreal` module. The project uses the UE 5.8 V7 build settings.
4. The Editor startup map is `/Game/SpacePatriot/Maps/L_KellenReachWalk`. Press **Play** with **Selected Viewport** to test the first-person character, rifle pickup, AI patrol/spawner and HUD inside the Editor. See `KELLEN_REACH_PLAYTEST.md` for controls, validation and known limits.
5. Open `/Game/SpacePatriot/Maps/L_KestrelFlight` to test the ship. `BP_KestrelGameMode` spawns `BP_KestrelFlyable`, which carries eight Kestrel parts and derives from native `ASPFlightPawn`. The grounded display ship is visible in the Editor and hidden during Play.

The mapped flight keys are W/S forward/back, A/D strafe, Space/Ctrl rise/descent, arrows pitch/yaw, Q/E roll, X brake, Shift boost, G gear, T assist, B cruise, P power, V camera and L request landing. F1 selects the Travel engineering preset, F2 Combat, F3 cycles NAV/SYSTEMS/POWER/MISSION readouts, F4 dims the readout, F5 starts the original Storyworks Water Ledger quest, and F6 saves society/campaign/survey state. Tab enables the clickable MFD pointer. The Kestrel and world Blueprints contain authored Event Graphs for these actions; see `BLUEPRINT_GAMEPLAY.md`. Native/API and Blueprint runtime checks pass, but a hands-on pass of every physical key is still needed. The ship bake and cockpit model remain provisional, and cargo interaction, visible citizens and settlements, original combat/inventory integration, sound, travel, and the full quest UI remain incomplete. See `PORT_AUDIT.md` for specific gaps and validation evidence.

## Project contents

- `Content/SpacePatriot/Blueprints/BP_KestrelFlyable.uasset`: native-flight player pawn with the eight Kestrel meshes.
- `Data/ExternalRuntimeContentManifest.json` and `SourceAssets/PolyHaven/manifest.json`: exact Epic template file checksums plus Poly Haven download URLs and checksums. `Tools/InstallLocalDependencies.ps1` recreates the required local `Content/SpacePatriot/OpenAssets/PolyHaven/` assets; those generated assets are not pushed to Git.
- `Content/SpacePatriot/Maps/L_KestrelFlight.umap`: dressed flight port, one bounded Earth `ASPWorldSurface`, and `BP_WorldRuntime` with society, Long Debt and Field Survey components.
- `Content/SpacePatriot/Maps/L_KellenReachWalk.umap`: separate port playtest with Epic's Arena Shooter pawn/controller, an explicitly configured rifle pickup, shootable NPC, AI spawner, nav bounds and HUD; the world runtime is preserved.
- `Content/Characters/`, `Content/Weapons/`, `Content/Variant_Shooter/`, `Content/FirstPerson/`, `Content/Input/` and `Content/LevelPrototyping/`: local UE 5.8 template Examples installed by the dependency script as replaceable mechanics placeholders; these folders are ignored by Git. See `FIRST_PERSON_ARENA_TEMPLATE.md` for provenance.
- `Content/SpacePatriot/Materials/M_WorldTerrainPBR.uasset`: optional near-field triplanar PBR geology material, assigned only to streamed world patches in the two port maps; the far globe keeps `M_WorldVertex`.
- `Content/SpacePatriot/Ships/KestrelK017/`: imported ship meshes and textures. The mesh was converted from X-forward/Y-up Unity art into Unreal's X-forward/Z-up coordinates; the symmetric wing and drive pairs occupy matching ±Y positions.
- `SourceAssets/KestrelK017/`: the reimportable, Unreal-oriented FBX and texture source. The original Blender project remains in the Unity repository.
- `Source/SpacePatriotUnreal/`: native flight, vessel engineering, bounded Worldworks terrain, society/cargo, Long Debt graph, Field Survey and tested architecture layout/route components, plus earlier Blueprint bases.
- `Data/Worlds.json`, `Data/CreatureRosters.json`, `Data/Worldworks/`, `Data/Settlements.json`, `Data/StoryCampaign.json`, and `Data/Architecture/DeckPlans.json`: source world, wildlife, terrain/climate, settlement, campaign and interior-layout data.
- `Tools/ImportKestrel.py`, `Tools/BuildPlayablePrototype.py`, `Tools/IntegratePlayableSystems.py`, and `Tools/SPWorldIntegrate.py`: reproducible Editor import, map construction and integration. Assets in `Content/` are committed, so opening the map does **not** require rerunning these scripts.
- `Tools/CreateBlueprintHandoff.py`: creates optional Blueprint subclasses from the native systems and a separate systems test map.
- `Tools/BuildShipGraph.py`: rebuilds and verifies the authored Kestrel and world Event Graph links through an Editor-only module; `Validation/ship-blueprint-graph.json` and `Validation/blueprint-runtime.json` record structural and runtime checks.
- `Tools/DressKellenReach.py`, `Tools/BuildKellenReachWalk.py`, and their validation reports: reproduce the port dressing and separate on-foot test map. `FAB_SHIP_OPTIONS.md` records free marketplace ship candidates without redistributing Fab source assets.

`Data/PlayableValidation.json`, `Data/PlayableSystemsIntegration.json`, and `Data/WorldRuntimeValidation.json` record Editor checks. `Validation/kestrel-native-flight-play.png` is a real game screenshot. The Kestrel's geometry still has visible bake seams and imperfect gear mounts; importing it into Unreal does not approve those art defects.

## Working with a friend

After cloning once, run `git pull` followed by `git lfs pull` to receive incremental changes. Unreal-generated `Binaries`, `DerivedDataCache`, `Intermediate`, and `Saved` folders stay local. Commit source changes plus deliberate `Content/` assets; Git LFS tracks `.uasset`, `.umap`, `.fbx`, `.png`, and Worldworks `.bytes` files.
