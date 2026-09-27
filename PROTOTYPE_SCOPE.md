# Space Patriot: system-complete internal prototype scope

This is the fast playtest target. It supersedes the simulation-heavy sequencing
in `UNREAL_FULL_PORT_AND_ASSET_PLAN.md`, while
`FULL_WORLD_PORT_INVENTORY.md` remains the record of original-game parity gaps.
The prototype should use licensed free packs as actual temporary art, not delay
playability until bespoke ships, terrain or characters are finished.

## Target experience and deliberate simplifications

In `L_KellenReachWalk`, the player can walk to a ship, board, fly, choose a
destination, jump, land at an active destination, exit, meet NPCs, fight, trade,
move cargo, take a quest, return, and see the same consequences after a save and
reload. Repeat that loop for the original 19 world IDs. A world ID selects a
**playable destination recipe**, not a physically simulated celestial body.
Each of the 420 settlement records can resolve to a stable, reusable district
layout with different modules, materials, props, services and residents; it
does not require 420 individually authored maps.

For this prototype, do **not** implement orbital motion, planetary rotation,
full spherical terrain, arbitrary planetwide landing, climate simulation,
weather physics, procedural geology, detailed ecology, water simulation, or
hundreds of unique ship and creature meshes. Use a local day/night cycle,
world-specific sky/material/lighting presets, terrain and vegetation **modules**,
and a few authored encounter rules. Travel can use a short in-ship jump/loading
transition into a destination sector. Landing needs a safe site or pad in that
sector; unrestricted landing across a spherical planet is not required for this game.
Static water, distant planet art and cosmetic weather VFX are acceptable where
they improve the scene, with no simulation dependency.

This is a scope change, not a claim that the original game's depth has already
been ported. Keep original data IDs, campaign text and faction/market state so
the prototype can grow without throwing away progress.

## Minimal architecture

1. Keep the current integrated map and its board/fly/travel code. Add one
   authoritative session state with stable world, settlement, ship, character,
   inventory, market, NPC and quest IDs. Use a single versioned save snapshot.
2. Resolve destination ID to a **recipe**: `WorldId + SettlementId + Seed +
   DistrictTemplate + BiomePalette + LocalSky + DayLength + ServiceSet +
   NPCRoster + EncounterSet`. A small template library supplies port, city,
   outpost, station and gas-habitat layouts. Deterministic placement varies
   modules/props; a per-site delta records only changed doors, stock, loot,
   NPC outcomes and quest flags. Active actors are spawned near the player.
3. Let gameplay own logic and packs own visuals. Semantic asset slots select
   a licensed mesh/material/animation/audio set for `ship.courier`,
   `district.industrial`, `npc.civilian`, `weapon.rifle`, `biome.ice`, etc.
   Ship hatch, gear, muzzle, seat and cargo sockets must be validated when a
   pack is assigned. Keep dependency installers/manifests local; the public
   repository carries project code, recipes and references rather than raw
   third-party pack archives.
4. Use lightweight offscreen **event ticks** for trade, jobs, travel and
   relationships. When entering a district, materialize nearby NPCs and ships
   from that state, run simple schedules/AI, then serialize results on exit.
   There is no offscreen movement or ecosystem simulation.
5. Expose a small set of shared gameplay events (board, depart, arrive, land,
   interact, buy, sell, load, fire, kill, talk, choose, complete). Market,
   inventory, combat, society and campaign respond to these events and write
   the same save state. This is the connection currently missing between native
   data components and physical play.

## System-by-system playable gate

| Original system / current Unreal base | Minimum prototype behavior that must work in play |
| --- | --- |
| Travel, navigation, world catalog: 19 IDs and one `SPPlayLoopDirector`/flyable Kestrel exist; hyperjump currently swaps a local surface profile. | Board, launch, select any of 19 IDs, jump to a matching destination sector, fly locally, land/dock, exit, reboard and leave. No orbit computation or 19 full globes. |
| Worldworks, Terrainworks, Grassworks, Oceanworks, Weatherworks: imported fields and a single surface/dressing patch exist. | Replace blank/proxy ground with reusable walkable terrain cards, ground materials, rocks, flora and sky presets for at least industrial, dry, ice and lush recipes. Each destination has deterministic layout and collision. One configurable day/night cycle; water/weather only cosmetic where selected. |
| Architecture, Pathworks, Machineworks: native layout tests and one Kellen Reach apron exist. | Generate navigable streets/rooms from 3–5 district templates; working doors, lifts/ramps, terminals, ship pads and a shop/cargo point. Every settlement ID maps to one template+seed. |
| Society: 420 records and 4,052 residents are simulated; almost none appear in the world. | Nearby civilians work, travel, converse or visit services on short schedules; one NPC owns a ship and arrives/departs; offscreen job/trade/relationship events advance cheaply and are visible on return. |
| Economy, freight, inventory: records and a freight terminal exist, but holds and player actions are not unified. | Buy/sell, accept/deliver a contract, physically load/unload cargo at a hold or terminal, enforce capacity and credits, update market stock and save it. |
| Campaign/Storyworks: nine-case graph is loaded; only the opening path has physical hooks. | Journal/dialogue/choice UI can show and complete at least one grounded objective path for **each of the nine cases** through real terminal, cargo, survey, combat or NPC actions. Consequences update state and survive reload. Additional narrative branches can stay in the graph until their encounters are authored. |
| Combat, weapons, Creatureworks: native weapon rules exist; map rifle and rock-shaped creatures are placeholders. | Ground and ship firing connect to actual targets, damage, loot/reputation/quest events and save. At least two creature archetypes plus one boss encounter reuse rigged packs and data-driven attack parameters. |
| Ships, vessel systems, interiors: one flyable Kestrel and vessel power/MFD code exist. | One small ship and one medium ship have boarding, seats, flight, gear, damage/power, cargo and navigable room(s). Spawn traffic from a reusable ship roster. Ten source chassis roles remain data/content expansion, not a gate for the first internal playtest. |
| UI, MFD, effects, sound: provisional working screen and partial event hooks exist. | Navigation, power, inventory, cargo, map, journal and trade are reachable with readable controls; button actions affect state. Basic audio/FX fire on launch, jump, shots, hits, doors, trade and mission updates. |
| Save/restore: several independent slots currently exist. | One save/load restores location, possession, ship, hold, inventory, market, NPC outcomes, quests and modified site state after Editor restart. |
| Multiplayer: original peer-hosted play has no integrated Unreal equivalent. | **After the solo prototype works**, add a minimal host-and-one-client board/fly/fight/trade test if co-op is needed for playtesting. It must share the same authoritative state; it does not block the first solo internal build. |

The pass condition is a **continuous playable journey**, not a list of JSON
records or test maps. Walk/board/fly/jump/land/interact/fight/trade/quest/save in
the existing project, first across two contrasting destination recipes, then
check that all 19 world IDs and all 420 settlement IDs resolve without missing
assets or broken services. A packaged Windows development build is the final
proof for playtesters; an installer and commercial polish are not in scope.

## Fast implementation order

1. **Unblock current gameplay**: an Earth→Mars auto-route/landing/exit/sample
   loop now passes live in Selected Viewport. Extend that check to manual
   flight, return travel, cargo, combat and save/reload; keep fixing controls,
   collision and screen readability before adding content. The older
   `Data/CohesivePlayValidation.json` predates this live pass and still reports
   `live_pie_verified: false`.
2. **Playable destination recipe**: reuse the current jump path but load a
   distinct modular district, sky, day/night preset and services for each
   destination. Preserve identity and per-site deltas. Avoid new tiny test maps.
3. **Core transaction loop**: connect freight, inventory, market, ship hold,
   on-foot and ship combat, NPC interaction and quest events to one state/save.
4. **Pack replacement pass**: install and select the free space, terrain,
   building, character, gun and ship packs already shortlisted in
   `UNREAL_FULL_PORT_AND_ASSET_PLAN.md`; reject packs that fail Unreal 5.8,
   socket/collision, license or style checks. Use palette variants to make
   destinations distinct quickly. Original custom art can replace slots later.
5. **Content coverage**: add the remaining district and biome recipes, nine
   quest paths, at least two ships and the creature/boss encounters, then run
   the complete travel/interaction/save matrix and package a Windows playtest.

The explicit rule for this milestone: spend engineering time on a player-visible
action or a state connection. Defer systems that only make the universe more
physically elaborate without making the prototype more playable.
