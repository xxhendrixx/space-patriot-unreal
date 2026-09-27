# Space Patriot: integrated Unreal port and free-asset plan

Prepared 2026-09-27. [PROTOTYPE_SCOPE.md](PROTOTYPE_SCOPE.md) is the active
internal-playtest plan: modular destinations, connected actions and a simple
day/night cycle. The larger sequence below is an archived survey of earlier
simulation ambitions, not a current implementation target. Its orbital,
climate, ecology, geology and full-globe work is deferred unless playtesting
shows a concrete need. The source
parity inventory is [FULL_WORLD_PORT_INVENTORY.md](FULL_WORLD_PORT_INVENTORY.md),
and the original browser game's modules are in
`../SpacePatriot/Reference/Original/source/`.

## Target and current blocker

Keep `/Game/SpacePatriot/Maps/L_KellenReachWalk` as the integrated play entry.
The target loop is: walk a populated district, board a ship, launch, choose a
route, jump to a **different moving body**, fly through its atmosphere or dock
at a gas-world habitat, land anywhere safe, explore and interact, return to
the ship, depart, and reload with the same world state. All 19 original worlds
in five systems and the 420 source settlement IDs must be reachable through
that loop. The engine should stream only the region around the player; it
should not load all planetary geometry at once or make 420 hand-authored maps.

Today `SPHyperjumpRouteComponent.cpp` changes the profile of one local
`ASPWorldSurface` and moves the pilot to an arrival point. `SPWorldSurface.cpp`
reads distinct world radius ratios but renders one fixed 18 km globe and uses
the exported 129×129 ground fields only around one small landing area.
Improving that globe's material alone cannot create the missing solar systems,
landable geography or cities. `SPJourneySave.h` and `SPPlayLoopDirector.cpp` do
not yet restore every society, story and survey consequence as one journey.

The look target is **grounded industrial science fiction**: legible ship
silhouettes, worn paint and functional construction; settlements built for
their climate and industry; quiet, restrained interfaces. Free packs supply
study examples and temporary components, not the game's identity.
[ORIGINAL_ASSET_WORKFLOW.md](ORIGINAL_ASSET_WORKFLOW.md) defines the
concept-to-Blender-to-Unreal process, provenance record and matched six-view
comparison used to make our own ships, structures and biome art. Compare every
candidate in the actual ship/city/planet context against approved concept
views before promoting it to production art.

## Port sequence and exit gates

| Stage | Work in the existing game | Gate before the next stage |
| --- | --- | --- |
| 0. Lock a reproducible baseline | Keep one canonical Unreal checkout and this integrated map. Record every original system, data file, existing C++/Blueprint entry point and runtime behavior from the inventory. Separate authored assets from optional local packs; add an asset registry and cook rules before replacing hardcoded `LoadObject` paths. | A fresh checkout opens the map with clear missing-pack fallbacks; the current walk/board/fly slice still works in Selected Viewport. |
| 1. Universe and travel | Port `living.js` and `celestial.js` world IDs, seeds, body types, gravity, orbital/spin frames and sun direction into an authoritative catalog/clock. Read `Data/WorldLocations.json` at runtime. Keep far positions in double-precision universe coordinates and convert only nearby actors to a body-local Unreal origin. Change hyperjump from in-place profile swap to arrival outside the selected destination. Preserve flight mode, ship state and passenger state across the transition. | Board at Kellen Reach, launch, jump Earth → Mars → a second system, approach each real target body, then return to Earth without changing play maps. Routes still resolve after reload. |
| 2. Planet surfaces | Port `expedition.js`/`planet-engines.js` cube-face cell addressing and type-specific geology. Generate collision and visible terrain from the same deterministic samples. Stream LOD cells around the ship/player, prefetch ahead, cancel stale work after world changes, and keep the old collision tile until its replacement is ready. Tie atmosphere, climate atlas, weather and water/lava to each world profile. | Land, exit and traverse at arbitrary safe coordinates on every solid world, with no inverted collision, visible seams, old-world tiles or dry-world oceans. Six gas worlds instead route to habitats. |
| 3. Settlements and services | Rebuild the active-district generator from `settlements.js`, `city-world.js`, Architectureworks, Pathworks and Machineworks. Key generated streets, interior rooms, doors, lifts, terminals and docking pads by the 420 stable settlement IDs. Use modular assets and PCG to dress these rules, not to decide gameplay geometry. | A city and an outpost on different worlds have distinct, walkable layouts and functioning trade/ship services. Leaving and returning preserves the same site. Batch validation covers all 420 IDs. |
| 4. Living systems | Connect Grassworks, Oceanworks, Weatherworks, Creatureworks, Spellworks and Fireworks to streamed body-local cells and event hooks. Render nearby NPCs and ships from the existing society/freight state; let offscreen state advance without spawning actors. Join combat, survey, cargo loading/unloading, manufacturing, market stock and all nine *Long Debt* cases through one stable event and item ledger. | A visible NPC completes a routine or trip; the player trades and physically loads cargo, encounters wildlife, completes a story choice, leaves and returns to see the consequences. |
| 5. Ships, interiors, UI and audio | Implement the original ten chassis roles and handling first, then variants. Replace temporary exteriors with art-approved hulls and matching collision, landing gear, hatch and cockpit. Build connected rooms and usable stations for medium and capital craft; multilevel interiors are an expansion beyond the original connected-deck implementation. Restore working MFD inputs, controls and original UI actions. Replace generic sound and visual effects through the same event hooks. | Every ship role can perform its intended journey/service action. Board, walk, sit, fly, land, unload, disembark and return without broken controls or a detached interior. |
| 6. Persistence and release proof | Consolidate journey, society, campaign, survey, inventory, ship and generated-site deltas in a versioned save. Validate cooking of soft-referenced local assets. Profile terrain/city transitions, draw calls, streaming hitches and memory; optimize where the real game exceeds the target-PC budget. | The full 19-destination travel matrix passes in **the same integrated game**, then in a packaged Windows build: approach, land/dock, interact, leave, save/reload. Installer work follows that result. |

Run each stage's checks through `L_KellenReachWalk` in Selected Viewport and
then the packaged game. A small asset preview project is acceptable for
checking an old pack's Unreal 5.8 compatibility; it is not another gameplay
world. Structural JSON checks and native unit tests are supporting evidence,
not substitutes for flying and walking the route. Current
`Data/CohesivePlayValidation.json` still says `live_pie_verified: false`.

## Free packs to study or use temporarily

These publisher pages displayed **Free** on 2026-09-27, except where the row
identifies a built-in or CC0/CC-BY source. They are candidates, **not installed
or accepted project dependencies**. Inspect the exact license at acquisition,
Unreal 5.8 compatibility, material quality, scale, collision, LOD and runtime
cost before promoting one. Prefer a consistent subset with custom material
instances over mixing whole demo scenes.

| Current placeholder / need | Candidate and intended use | Limit or required adaptation |
| --- | --- | --- |
| Flat or generic orbital backdrop | [Starfield FREE](https://www.fab.com/listings/349d203e-aac2-40bc-8c92-7a3f6f89cf31) for distant stars/nebulae; Unreal [Sky Atmosphere](https://dev.epicgames.com/documentation/unreal-engine/sky-atmosphere-component-in-unreal-engine) for ground-to-space scattering. | Use actual target stars and world lighting from the universe catalog. Starfield's listing names a Virtual Camera plugin for dynamic materials; check that dependency and 8K texture cost. |
| Generic distant planet material and rings | [Solar System Scope planet maps](https://edu.solarsystemscope.com/textures/) for recognizable Solar bodies; [NASA 3D Resources](https://science.nasa.gov/3d-resources/) for reference/selected Solar maps; [Epic Cassini Sample](https://www.fab.com/listings/3f7cd12c-30b3-47d6-90c2-8604ed068ab7) for ring/station art and PCG ideas. | Orbital maps do **not** supply landable geometry or fictional exoplanet geography. Solar System Scope is CC BY 4.0, so credit and note edits. Review each NASA item's rights and acknowledge NASA without implying endorsement. |
| Single-looking ground and rock proxies | Existing [Poly Haven](https://polyhaven.com/license) CC0 assets plus [lunar regolith](https://polyhaven.com/a/moon_01) and [moon rocks](https://polyhaven.com/a/moon_rock_06); [Open World Demo Collection](https://www.fab.com/listings/3262ab8f-f64a-4124-8efd-82cb19df6249) for selected cliffs/foliage. | Materials and meshes dress the deterministic source geology; they cannot replace terrain generation. Make climate-specific material sets and collision-aware foliage subsets. |
| Boxy port, cities and habs | [City Sample Buildings](https://www.fab.com/listings/008fe959-5511-428e-93bd-f99b1179f6d5) as selected structural modules; [UNIBLOCKS FREE](https://www.fab.com/listings/e36c2bc1-49d4-4918-a0d7-f09a90ec7a57) as a possible modular shell kit. | City Sample is modern terrestrial architecture and UE-only, so select, retexture and industrialize only useful modules. UNIBLOCKS is rectangular and expects Nanite/Lumen. Neither generates the original 420 functional destinations. |
| Bare ship/station interiors | [Modular SciFi Season 1](https://www.fab.com/listings/86913335-3c75-42bf-8404-54fe9d9d7396) and [Season 2](https://www.fab.com/listings/cb3c4494-4060-4a80-b079-e46936cb8dd0) for hallways, command spaces and props. | Originally UE4-era content: verify migration to UE 5.8, scale, pivots, material response and draw cost in an isolated import. Recompose into ship-specific room layouts from the source; do not copy example maps wholesale. |
| Static civilian placeholders | [City Sample Crowds](https://www.fab.com/listings/903037e9-e1ac-4f41-96e8-1683c6fa7ad4) for rigged civilian variants. | UE-only and potentially expensive; nearby agents need real schedules/interaction, while distant residents stay simulated records. Retexture/wardrobe variants by world and faction. |
| Rock-shaped hostile wildlife | [TEUTHISAN](https://www.fab.com/listings/04d8f11b-ddf6-4e0c-835b-9648e8b4379d) as one rigged predator and animation test. | One creature is not a fauna system or ten bosses per planet. Gameplay uses seeded species/ability data; curated creature families and original art still need production. |
| Crude small-ship exterior | [DROPSHIP R35](https://www.fab.com/listings/357a3c30-e07c-4cf0-a189-744ffecca5f8), [EZNO](https://www.fab.com/listings/72032a08-8b7d-4b8c-b6ec-31c596de3f00), [Star Sparrow sample](https://www.fab.com/listings/932ce79b-06e8-416a-ba0d-36435621a0a1) as side-by-side small-craft candidates. | R35 has separable gear/hatch/engines but no rig, LOD, FX or interior. Star Sparrow warns it is not for close shots; the free listing is a sample, not the paid collection. None fills the medium/capital fleet or matches the approved concepts by itself. Keep the current cruiser a temporary dependency until an in-game replacement passes silhouette and function checks. |
| Generic HUD and weak effects/sound | Reuse the original game's owned UI/art direction and event definitions. [Kenney Sci-Fi UI](https://kenney.nl/assets/ui-pack-sci-fi) and [Sci-fi Sounds](https://kenney.nl/assets/sci-fi-sounds) are CC0 sources for temporary controls/cues. | Kenney's graphic style is too simple for final cockpit screens. MFD buttons and knobs must call actual navigation, sensor, engineering and communications functions; a screen texture alone is not a port. |

Do **not** budget [PlanetX](https://www.fab.com/listings/d64ca545-302a-4158-9abd-f806904a4178)
as the planetary engine. Its free listing explicitly says runtime World Partition
ground entry is unsupported. It may help with distant/orbit visuals after a
compatibility spike, but the source-specific terrain, collision and settlement
streaming remain our work. The Fab item titled
[[FREE] Planet Project](https://www.fab.com/listings/fc591129-a147-4684-acbb-38a990e30c3a)
lists UE 5.3/5.4 and currently needs a fresh $0/license check before it can
enter this no-cost shortlist; it is not an assumed dependency. Do not assume
all Quixel/Megascans items are free.

## Asset pipeline and art acceptance

Extend `Tools/InstallAllDependencies.ps1` and the existing
`Data/ExternalRuntimeContentManifest.json` rather than adding downloaded packs
to public Git. For each adopted asset record its exact URL and publisher,
acquisition date, displayed price, acquired license/receipt, attribution,
source hash, import recipe, engine version, destination package path and an
offline fallback. Put the selection behind stable semantic slots such as
`Ship.Small.Courier`, `World.Mars.Orbit`, `Biome.Ice.Cliff`,
`Settlement.Industrial.Wall` and `Creature.Predator.01`. Resolve these with
soft asset references or an Asset Manager registry, not literal paths in
`SPFlightPawn.cpp` and `SPWorldDressing.cpp`. Explicitly cook every selected
runtime asset and verify it in a packaged build; Editor success is not enough.

For each replacement, capture matching front/side/top and in-game camera
views beside the approved concept reference. Reject candidates with the wrong
silhouette, axis, scale, hatch/gear attachment, interior proportions, UVs or
material response even if they import cleanly. Check cockpit visibility while
piloting, LOD changes during approach, and collision while landing/boarding.
Paint, decals, grime and lighting may unify compatible packs; they cannot fix
wrong geometry or gameplay sockets.

[Fab's Standard License](https://www.fab.com/eula) permits embedding content
in a commercial game but restricts redistribution of raw assets; import such
packs into each authorized collaborator's local library and publish only the
manifest/script and our own work in the public repository. [Poly Haven is
CC0](https://polyhaven.com/license); [Kenney's licensing guide](https://kenney.nl/support)
also identifies its game assets as CC0. Keep required Solar System Scope credits
in the game and repository. A free price is not a grant to re-host a pack.

## Immediate implementation order

1. Freeze the existing integrated map as the acceptance route and add a
   repeatable Selected Viewport play checklist: spawn → freight/interaction →
   board → launch → jump → approach → land/dock → disembark → save/reload.
2. Add the universe catalog/clock and replace the in-place surface swap. This
   unlocks meaningful planet art and validates that the player reaches the
   intended world.
3. Study the **space sky, one Solar orbital body, one terrain biome, one city
   kit and one ship/interior set** from the shortlist. Record general methods
   and shortcomings, then build Space Patriot versions from the original
   systems and approved concepts. Use any pack content that remains in the
   runtime only through the manifest and asset registry, with its license and
   provenance intact. Compare each result in the integrated map.
4. Build deterministic landable cells, then district generation and persistent
   local state. Expand the asset sets by climate and ship class as those systems
   become reachable.
5. Port the remaining engine behaviors and run the all-world matrix before
   calling the game or its art pass complete.

This order avoids spending time populating a planet that is only a renamed
globe or approving a good-looking asset that cannot survive cooking, landing,
boarding or an editor restart.
