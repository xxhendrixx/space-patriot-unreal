# Combatworks native slice

`USPCombatSimulationComponent` ports the bounded simulation rules from `Reference/Original/source/combat.js`, cross-checked against Unity's `FrontierWeapons.cs`. This is Unreal C++ gameplay state and Blueprint-callable API, **not a playable combat encounter yet**. No ship gun meshes, ground weapons, muzzle VFX, impact art, HUD/MFD pages, encounter rewards, sound, boss abilities, or map actors were added here.

The five source weapons retain their names, damage, cadence, magazine, reserve, reload, range, heat and speed. The component also handles ship/ground/turret selection, SCM/power arming gates, cooldown, laser capacitor, heat/overheat hysteresis, automatic fire, target cycling, intercept calculation and seeker lock, guided missiles, swept projectile-sphere hits, shields before hull, suit damage, shield regen delay, service/rearm, deterministic raider/sentry firing, and a save snapshot with validation. `DrainEvents` emits fire, impact, destruction, player-disabled, reload, and one-time sector-clear events for presentation or game progression.

Positions, radii and velocities in this API are **metres** and **metres per second**. Unreal actor locations are centimetres: divide actor/world positions by 100 on input, multiply event/projectile positions by 100 for visual output. The source game stored kilometres; the weapon constants above were multiplied by 1,000 while porting. Run `StepCombat` at a fixed 30–60 Hz cadence with `DeltaSeconds <= 0.1` and feed in the current flight pose, walking/turret state, power/mode and vessel factors. It deliberately does not tick itself. Supply `bAuthority=true` only on the authoritative game instance; clients can replay host snapshots and create local shot events but cannot apply damage or run NPC tactics. The event queue and projectile list are bounded at 128 and 64 records, respectively.

Integration still required:

1. Attach the component to the flyable ship/combat controller and connect flight inputs, ship mode and `USPVesselSystemsComponent` power/cooler/shield factors.
2. Connect actual actors for contact positions/radii; convert metres/centimetres at that boundary. Connect `TryFire`, reload, target and armed controls plus MFD/readout state.
3. Render guns, projectiles, beams and impacts from state/events, including sounds and authored VFX. Add environment collision/line-of-sight tracing: this isolated core only traces contact and player spheres, so terrain and station walls do not yet stop fire or seeker acquisition.
4. Route destroyed-contact events into Storyworks, society/reputation and encounter rewards. Connect save snapshots to the game SaveGame and authoritative replication to the eventual multiplayer system.
5. Validate live aiming, frame cadence, balance and collision at ship and ground scale in the playable map.

Validation: build `SpacePatriotUnrealEditor Win64 Development` and run `UnrealEditor-Cmd.exe <project.uproject> "-ExecCmds=Automation RunTests SpacePatriot.Combat; Quit" -unattended -nop4 -nosplash -nullrhi`. Three native automation tests cover source weapon constants, reload conservation, laser capacitor, missile lock and damage, shield overflow/regen, deterministic AI firing, save corruption rejection, authority gating and overheat recovery.
