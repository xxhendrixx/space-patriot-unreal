# ArchitectureWorks / ship interior port slice

`ASPArchitecture` is a native, Blueprint-facing implementation of the original
ship deck document and a bounded part of ArchitectureWorks geometry. It reads
the **byte-identical** Unity `DeckPlans.json` source copied to
`Data/Architecture/DeckPlans.json` (SHA-256
`50fb9a59323f504888b28658fcd4d1636406cd4eca8181890d51f1d949484482`).
The source has 10 interior families. Family 9 is the large multi-deck reference:
4 decks, 53 rooms, 52 bulkheads, 176 fixtures, and a lift stop on every deck.

Spawn `/Script/SpacePatriotUnreal.SPArchitecture`, set `InteriorFamily` to the
vessel's source `interiorFamily`, and call `RebuildFromSource` (or enable
`bLoadOnBeginPlay`). Attach the actor to a cabin root if the vessel moves.
The source x/y-up/z-aft metres map to actor-local Unreal
`(x * 100, -z * 100, y * 100)` centimetres. Query locations use a character's
**feet** position, not capsule centre.

Blueprint hooks and calls:

- `RoomAtLocalLocation`, `CanOccupyLocalLocation`, `FindRoomRoute` expose the
  source rooms, measured bulkhead openings, solid fixture bounds and deck
  circulation. `FindRoomRoute` returns source room IDs in travel order.
- `SetDoorOpen` / `IsDoorOpen` change the physical door panel and route graph;
  `OnDoorChanged` reports a state change. Doors begin open, as in Unity's
  `PrepareDeck`.
- `SetLiftPower`, `RequestLiftToDeck`, `AdvanceLift`/Tick and `OnLiftArrived`
  expose the service lift. With power off, the platform holds position and
  cross-deck routes are unavailable. A Machineworks power consumer can call
  `SetLiftPower` when that engine's graph is ported.
- `FloorMaterial`, `BulkheadMaterial`, `FixtureMaterial`, `DoorMaterial` are
  art slots. Floors, ceilings, bulkheads and solid/decorative fixtures are
  separate instanced components; doors and the lift platform are movable
  components. All dimensions are from source data, with 2.8 m wall height and
  0.16 m wall thickness within the source deck spacing.

The family-9 geometry has 571 static instances, below the 2,048-piece budget.
Headless Unreal runtime validation passed 22/22 checks, including the route
`bridge → corridor-0 → corridor-1 → corridor-2 → corridor-3 → cargo-3-0--1`,
closing the cargo door, cutting lift power, lift arrival, and small-family
reload. See `Data/Architecture/ArchitectureRuntimeValidation.json` and rerun:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' SpacePatriotUnrealEditor Win64 Development '-Project=<path>\SpacePatriotUnreal.uproject' -WaitMutex -NoHotReloadFromIDE
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' '<path>\SpacePatriotUnreal.uproject' -unattended -nop4 -nosplash -nullrhi '-ExecutePythonScript=<path>\Tools\ValidateArchitectureRuntime.py' -log
```

This is **not a finished cabin or city**. The cube mesh establishes source
dimensions and collision; it needs authored hull, wall, ceiling, hatch,
fixture, surface, glass, light and signage assets, plus a material and trim
pass. Door motion currently snaps instead of animating. The actor is not yet
attached to the playable ship or paired with first-person controls. The four
ArchitectureWorks building models in `Buildings.json`, the 420 settlement
placements, exterior site geometry, furniture/room compiler, pedestrian
Pathworks network, and Machineworks reactor/battery/signal simulation still
need separate ports. The lift's power switch is an integration hook, not a
claim that Machineworks itself is running.

`Data/Architecture/DeckPlans.json` is a non-asset JSON source outside
`Content`. Editor and development builds can read it from the project folder;
packaged builds must stage it as a non-asset file or migrate it to a cooked
asset. Treat that packaging task as required before a distributable build.
