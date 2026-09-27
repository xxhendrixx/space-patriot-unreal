# Full-game port audit

Audited 2026-09-27 against the original browser source, the Unity restoration, the Unreal C++ source, committed `Content/` assets, and actual Unreal build/Editor checks. The original contains 91 JavaScript/GLSL source modules and twelve supplied engine applications (thirteen extracted runtimes). A loaded JSON catalog or Blueprint API is **not** equivalent to a playable engine.

## Current evidence

| Area | Unreal status | Evidence and missing behavior |
| --- | --- | --- |
| Project and ship | **Playable slice** | UE 5.8 native module compiles. `L_KestrelFlight` and `BP_KestrelFlyable` load the symmetric Kestrel with eight mesh components and its materials. The Blueprint now derives from `ASPFlightPawn`; the static display ship is hidden during Play. The other original craft, damage/animation, cargo attachments and large ships are absent. |
| World catalog and travel | **Data/API only** | Nineteen world identities and 190 wildlife definitions load. There is one authored flight map. Celestial orbits, system travel/jump, world transitions and physical placement of the other destinations are absent. |
| Terrain and climate | **Bounded native slice in map** | `ASPWorldSurface` reads all 13 solid-world heightfields and 19 climate atlases. One Earth actor and a vertex-color material are saved in `L_KestrelFlight`; the headless 19-world activation check passed with `integrated_map_actor=true`. The five-province geology blend, spherical terrain/collision parity, atmosphere, sky, clouds, water, weather and biome visuals remain incomplete. |
| Vegetation and ecology | **Catalog only** | The 190 roster entries and a health/damage Blueprint base exist. No Grassworks/forest/Gaussian vegetation, creature meshes, spawned animal lives, boss abilities, animations, food webs or visual ecology are running in the map. |
| Society, economy and cargo | **Native simulation in map, presentation pending** | `USPSocietySimulationComponent` passes source-parity automation for 420 settlements, 19 markets, 4,052 seeded residents, jobs, relationships, freight/cargo transactions and compact save state. It is attached to `BP_WorldRuntime`, but visible NPC actors, ships, terminals, dialogue, menus, weapons and city services are absent. The 420 names are not 420 rendered cities. |
| Flight and cockpit | **Native pawn in map; live controls under review** | `ASPFlightPawn` compiles, passes 11 headless API/launch checks, and is the Kestrel Blueprint's parent. It maps six-axis movement, boost, brake, gear, assist, cruise, power, camera and landing actions. Three working 3D MFDs, physical knobs/switches, readable menus and a cockpit-matched mesh are absent. The original `systems.js` vessel engineering (12 power segments, presets, component health/heat/repair/pressure) is not ported. |
| Combat and weapons | **Hook only** | Wildlife health accepts damage in C++. Original ship/ground weapon meshes, targeting, projectiles, shields, heat, impacts, enemy tactics, bosses and encounter rewards are not connected. |
| Campaign and Storyworks | **Native graph in map; play wiring pending** | The original *The Long Debt* has nine cases, 90 graph nodes, 27 milestones and outcome consequences. `USPStoryCampaignComponent` compiles and a full-graph test traverses both ending branches with gates, rewards and a save roundtrip. It is attached to `BP_WorldRuntime`, but dialogue actors, terminal/sample/combat event wiring, UMG/MFD choices and actual player progression are absent. The original per-world `field-story.js` survey quest is a separate large Storyworks system; a native source-parity slice exists, but scanner, site, sampler, Spellworks and UI interactions remain unwired. |
| Architecture, machinery and paths | **Native layout test; map integration pending** | `ASPArchitecture` reads the original DeckPlans; family 9's four decks, 53 rooms, 52 doors, powered lift and route blocking passed headless tests. It is not attached to the playable Kestrel, and its box geometry is a layout/collision prototype. City room compilation, polished interiors, machinery graphs, pedestrian paths and 420 settlement scene placements remain absent. |
| Inventory and crafting | **Partial data/state** | The society component handles three freight goods and transaction conservation. Original equipment, consumables, field sampling, ammo/reload, recipes, crafting stations and repair interaction are not integrated. |
| Multiplayer | **Entire system missing** | The original peer-hosted session/snapshot/authority/chunking path has no Unreal networking equivalent. Neither replication nor co-op station play is implemented. |
| Audio, UI and VFX | **Major systems missing** | No authored audio mix, in-game menus, MFD drawing, Spellworks effects, Fireworks emitters or original HUD/campaign screens are playable in Unreal. |
| Delivery | **Editor only** | Unreal C++ Editor build and map tests exist. No standalone Windows player, installer, save migration, long-session stability or release performance pass has been verified. |

## Engine-by-engine check

The source engine applications are `Grassworks`, `Oceanworks`, `Fireworks`, `Spellworks`, `Worldworks`, `Storyworks`, `Terrainworks`, `Architectureworks`, `Pathworks`, `Machineworks`, `Weatherworks`, `Inventoryworks` and `Creatureworks` (Worldworks and Storyworks are extracted from one application). **No complete source engine is ported to Unreal yet.** Worldworks/Terrainworks have a bounded native data/mesh subset; Inventoryworks has freight transaction subsets; Storyworks has the *Long Debt* graph and a tested Field Survey slice without playable presentation; Architectureworks has a tested layout/route/lift slice. The rest have source references or catalog rows only.

The Unity project has additional implementation that can guide the Unreal port, but Unity itself is also incomplete. Its 100 original craft, spherical vegetation/sky work, 31-click cockpit MFD validation, cargo/interior and society systems should be used as behavior and art references. Asset presence or a passing isolated test does not establish that the Unreal player can experience those systems.

## Completion gates

1. Build and open the primary Unreal project, enter Play, fly and land the Kestrel with the native controls, and verify cockpit/menu interactions.
2. Put the world surface and society components into the playable map; travel among all 19 worlds while terrain, collision, climate, sky, water, vegetation and nearby cities stream with measured bounds.
3. Import or rebuild the original 100-ship fleet, connected interiors, weapons, machines, cargo mechanisms, NPC vessels and diverse ecology; approve every art asset against orthographic references and gameplay views.
4. Port the nine-case campaign graph, jobs, dialogue, economy, inventory/crafting, combat, survival systems and persistent consequences through actual play.
5. Rebuild peer-hosted multiplayer authority/replication if full original-game parity is required, then validate multiple players and long-running simulation.
6. Verify each system in an integrated packaged Windows build, including performance, controls, sound, saves, visual quality and regressions.

The next integration priorities are live flight and terrain verification, physical cockpit controls, vessel engineering, and visible NPC/city/mission interactions. Multiplayer is not hidden behind the current map; it requires substantial new implementation.
