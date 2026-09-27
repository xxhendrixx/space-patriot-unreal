# Original Space Patriot → Unreal: full-world port inventory

Audited 2026-09-27 against the archived original `Space_Patriot.html` and its
modular `source/` tree, copied from the Downloads archive to
`../SpacePatriot/Reference/Original/`. This describes the **local Unreal working
tree**, which currently contains work not yet on GitHub `main`. It is an
implementation inventory, not a claim that catalog data or passing unit tests
make the 19 destinations playable.

The original is a curated **19-world, five-system** game (13 solid worlds,
six gas worlds): Sol (8 worlds),
TRAPPIST-1 (7), TOI-270 (2), K2-141 (1), and WASP-76 (1). It generates and
streams local terrain, ecology, and active districts around the player; it does
not keep every planet's geometry resident. Its settlement directory contains
**420 destinations**: 192 cities/habitats and 228 outposts. Its fleet defines
**10 chassis × 10 variants**. These are the parity targets, with the source's
stylization and limits understood.

## Required port work, in dependency order

| Priority | Original behavior and evidence | Unreal status now | Work required and completion check |
| --- | --- | --- | --- |
| P0 | **World identity and spatial simulation.** `source/living.js`, `cosmoplot-data.js`, `celestial.js`, `assets/data/cosmoplot-worlds.json`: stable world IDs/seeds, distinct compressed radii and gravity, orbital positions, rotation, sun direction, stations and gas habitats. | `SPWorldSurface.cpp` loads all 19 IDs and climate atlases, but uses one fixed 18 km globe; the imported radius ratio is unused. `Data/WorldLocations.json` exports positions that the runtime does not read. | Make the world catalog and clock authoritative in Unreal. Preserve IDs/seeds and body classes; update scale, gravity, orbital/spin frames and attached actors. A route must resolve to the same moving body before and after save/load. |
| P0 | **Flight and travel.** `source/expedition.js`, `celestial.js`, `shipboard.js`, `flight-ui.js`: SCM/NAV, manual approach, atmosphere entry, takeoff, safe landing, docking, services and on-foot egress. | One Kestrel pawn can board/fly. `SPHyperjumpRouteComponent.cpp` changes the active surface profile and teleports to a fixed local arrival point; it does not travel through the source system layout. | Connect all 19 destinations to the spatial model. After jump, leave the pilot in flight outside the destination atmosphere; let them fly in, land or dock, leave the ship, return and depart. Gas worlds use habitats. Validate the complete journey in the integrated level, including save/reload. |
| P0 | **Planet geology and streaming.** `source/expedition.js` (`Geology.sample`, cube-face cells), `planet-engines.js`, `engines-worldworks.js`, `engines-terrainworks.js`: type-specific craters, dunes, ice, volcanic relief, deterministic 129×129 regional fields, matching terrain and collision, bounded tile cache/prefetch. | `SPWorldSurface.cpp` imports the 13 solid-world fields but blends each only around one 3 km landing region. Elsewhere the globe uses broad noise; one 6 km detail patch follows the player. | Port the type-specific geology and per-location terrain jobs. Stream deterministic collision and material LOD around every approach, preserve the previous patch while a replacement prepares, cancel stale jobs on world change, and permit safe landing away from the first port. |
| P0 | **Settlement destinations.** `source/settlements.js`, `city-world.js`, `engines-architecture.js`, `engines-pathworks.js`, `engines-machineworks.js`: 420 records, active-district geometry, 16 multilevel buildings per city, two per outpost, connected streets, furnished rooms, colliders, powered lifts and machines. | `SPSocietySimulationComponent.cpp` has settlement records but no corresponding walkable cities. Kellen Reach is one authored apron with placeholder actors. | Turn settlement records into streamed districts. At each selected city/outpost, generate a coherent site with collision, entrances, navigable paths, interiors, terminals, lifts and services. Preserve site identity and state when leaving and returning. Do not hand-build 420 static maps. |
| P0 | **Shared persistent state.** `source/living.js`, `campaign.js`, `economy.js`, `society.js`, `shipboard.js`: player, ship, active world, cargo, stock, reputation, story and home state survive return trips. | Journey, society, campaign and survey have separate save paths; not every visible entity or world site is restored. | Define one versioned save contract and stable IDs for generated content. A save made after trading, damaging a ship and changing a quest must restore those facts after a jump and Editor restart. |
| P1 | **Planet-specific climate and materials.** `source/climate-data.js`, `climate.js`, `visuals/environment.js`, `world.frag.glsl`, `assets/textures/`, `docs/FRONTIER_11.md`: climate fields, geology/soil/ice/lava textures, clouds, atmosphere-to-space transition and local star lighting. | Nineteen climate atlases exist, but remote sites share generic globe/material treatment and abrupt profile swaps. Local Poly Haven materials are temporary. | Drive terrain, sky, atmosphere, thermal/radiation cues and lighting from each world profile. Use or deliberately replace the original atlas families; verify dry Venus, icy/airless worlds and gas habitats look and behave differently. |
| P1 | **Grassworks, forests, Oceanworks and Weatherworks.** `source/engines/{grassworks,oceanworks,weatherworks}.js`, `visuals/{vegetation-stage,forest-stage,weather-stage,cloud-platform}.js`: stable 3×3 grass tiles, forest forms, wind, precipitation, water waves/foam and distinct lava treatment. | `SPWorldDressing.cpp` places 26 rocks and 0/8/16 plants near one remote arrival area. There is no equivalent climate-driven ecosystem stream, animated water or weather. | Port bounded local generation tied to terrain samples and climate. Stream roots and collision consistently, cull distant vegetation, keep effects attached to rotating worlds, and prevent dry worlds from showing water. |
| P1 | **Wildlife and combat.** `source/biology.js`, `engines/creatureworks.js`, `visuals/fauna.js`, `combat.js`: seeded fauna phenotype, gait/hit volumes, attacks, deaths, ship and ground weapons, damage and rewards. | Wildlife definitions exist in data, but two remote encounter actors use rock meshes; the map's rifle/guards are Epic Shooter placeholders and are not wired through all original rewards/inventory. | Give animals actual meshes/animation/AI, aggression and combat abilities; route all hits, loot, faction effects and death persistence through one combat model. Include playable encounter and boss-design additions separately from original parity. |
| P1 | **Society, economy and traffic.** `source/society.js`, `economy.js`, `settlements.js`: faction actors, civilian work and dialogue, tactical patrols, world stock/production, trade, convoys, contracts, ship services and reputation. | Native society/market simulations hold thousands of records and freight state. They do not place independent civilians/ships or show most simulated events in the active district. One freight terminal exposes a narrow contract path. | Spawn only nearby actors from the persistent simulation, give them job schedules, paths and usable ships, and make market/convoy/faction events visible and interactable. Advance offscreen events without rendering them; reconcile on return. |
| P1 | **Cargo and inventory.** `source/economy.js`, `engines-inventory.js`, `field-inventory.js`, `shipboard.js`: cargo capacity, buy/sell, deliveries, fabrication, ammunition and repair parts. | Inventory and freight have native models, but there is no physical cargo loading/unloading loop or full ship hold integration. | Link port terminals, holds, cargo transfer, purchase, fabrication, repair and quest milestones to the same item ledger, with capacity/ownership checks and persistent world stock. |
| P1 | **The Long Debt and side objectives.** `source/campaign-data.js`, `campaign.js`, `campaign-ui.js`, `engines-storyworks.js`, `field-story.js`: nine cases, 90 nodes, 27 world-event milestones, 18 ending choices with credits/stock/reputation effects. | The graph is loaded/tested, but in-map play exposes the opening case and a freight-service signal only. There is no full dialogue/choice/journal flow. | Wire terminal, sample, power, cargo, station and combat events to every case. Present journal, dialogue and choices in world and ship UI; apply the chosen consequences and restore them from save. |
| P2 | **Fleet, interiors and engineering.** `source/expedition.js`, `systems.js`, `interior-layout.js`, `shipboard.js`, `visuals/{craft,large-craft,interiors,instruments}.js`: 100 variants, medium/capital hulls, connected deck, seated stations, engineering power/heat/repair, crew boarding and turrets. | One boardable Kestrel and a visual cruiser placeholder; native vessel/deck-plan data exists, but no walkable multi-room ship or fleet traffic. | Implement the 10 chassis roles and handling before skin variants, correct collision/landing gear and station positions, then connected rooms, airlocks, seats, engineering and cargo. Verify moving-ship walking, boarding and egress. The original large ships have one connected deck, not full multideck parity. |
| P2 | **Presentation, effects and audio.** `source/visuals/`, `source/engines/{spellworks,fireworks}.js`, `source/{flight,mfd,pause,city,society,campaign}-ui.js`, `assets/ui/`: MFDs, route feedback, impact beams, finite fire, thrusters, cockpit/weapon art and sound cues. | Provisional HUD/MFD, placeholder assets and little world-specific audio; most Spellworks/Fireworks events are not visible. | Port the game-used event hooks and readable screens/controls, then replace temporary visuals/audio with licensed or original production assets. Spellworks in the source is used for combat/survey effects, not a player spellcasting feature. |
| P2 | **Multiplayer.** `source/multiplayer.js`, `shipboard.js`: peer-hosted combat, occupants, engineering/turret stations and active-district snapshots. | No equivalent integrated Unreal session flow. | After the solo universe works, choose and implement an Unreal host/session architecture with authority, replication, persistence and join/reconnect tests. The source is bounded peer-hosted play, not an MMO. |

## Full-world acceptance gate

Use the **same integrated play project**, not new isolated test worlds. Check
each of the 19 catalog destinations for: correct body/system and seed;
approach from space; visible atmosphere/habitat and distinct terrain/climate;
safe land/dock; an explorable settlement or outpost with usable services;
nearby ecosystem/encounter behavior appropriate to body type; return to the
ship; departure to another system; and save/reload preserving player, ship,
market, NPC and story state. For gas worlds, substitute a walkable habitat for
ground landing. Run performance checks while moving between cells and districts.

The original source's `scripts/planet-flight-acceptance.mjs` and
`scripts/feature-behavior.test.mjs` provide concrete continuity checks for
moving planets, manual arrival, terrain job cancellation and vegetation
attachment. Unreal's saved-map reports currently validate structure, not
this full journey. `Data/CohesiveWorldValidation.json` passes 90 checks, while
`Data/CohesivePlayValidation.json` still states `live_pie_verified: false`.

Previously requested **ten bosses per planet, fully authored intricate cities
everywhere, and multilevel capital-ship interiors** extend beyond the archived
game's implemented scope. Track them as expansion design work after the
corresponding original gameplay systems are connected; do not count them as
already ported because their catalogs or concepts exist.
