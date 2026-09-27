# Unity → Unreal system map

This map separates source behavior from work still needed in Unreal. The opening `L_KestrelFlight` map contains an imported Kestrel and one native world runtime. The catalogs load in the Editor, but the source world engines and most gameplay are not yet running there.

| Space Patriot source system | Unreal handoff | Status |
|---|---|---|
| 19-world catalog and climate identity | `FSPWorldRecord`, loaded from `Data/Worlds.json`; one `BP_WorldRuntime` actor in `L_KestrelFlight` | Catalog load verified in Editor; source world meshes, atmosphere transition, oceans, weather, and terrain streaming are not yet in the baseline map |
| Wildlife and bosses | 190 source creature records; per-world roster query; persistent encounter health and Blueprint health event | Catalog/combat hook ported; meshes, locomotion, telegraphs, abilities, reward drops, and boss arenas still need BP/animation work |
| NPCs with jobs and routines | Deterministic daily role/activity choice with job, needs, seed, and threat input | Decision hook ported; pathfinding, schedules, relationships, and persistent NPC life histories still need implementation |
| Society and story simulation | Seeded compact daily story events for trade, romance, intrigue, alliance, betrayal, family disputes, rescue, and business outcomes | Event substrate ported; quest chains, dialogue, factions, death/rebirth, and save-game history still need content |
| Flight and cargo | Kestrel K-017 imported as eight textured meshes and a `DefaultPawn` Blueprint for temporary free flight; native `ASPPlayerShipPawn` exposes cargo state separately | The ship is visible in Play, but its chase placement/collision are temporary; source flight physics, gear motion, cargo doors, and controls are not yet wired together |
| Cockpit/MFD interaction | Blueprint MFD base with page navigation, brightness, and a control-changed dispatcher | Interaction API ported; screen materials, instrument drawing, physical knobs/buttons, and cockpit layout still need assets and BP graphs |
| Weapons and wildlife damage | Wildlife encounter accepts weapon-hit damage and broadcasts health | Basic hit hook ported; weapon selection, projectile behavior, effects, sound, and balancing remain |
| Economy, settlements, lifts, buildings | Fields and event categories reserve integration points in the data model | Not yet ported; requires the next implementation pass |

## Data and simulation rules

- Use the stable `id` fields in the JSON catalogs as save-game and actor identity. Do not use display names as keys.
- World and character decisions use deterministic seeds so an unloaded simulation can advance compactly without spawning every NPC.
- Keep one authoritative world simulation actor per loaded map. It advances daily events; individual citizen actors animate only when near the player.
- Keep creature definitions in data and encounter state in actors. Use the Blueprint dispatchers to drive MFD warnings, VFX, audio, and UI.
- Avoid per-frame Blueprint loops for whole-world simulation. Use scheduled, event-driven daily steps and actor-level ticking only for nearby presentation.
