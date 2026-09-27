# InventoryWorks / field kit native port

`USPInventoryComponent` is a Blueprint-spawnable, native state machine based on
`Reference/Original/source/engines-inventory.js` and its game adapter,
`field-inventory.js`. It also preserves the core field crafting numbers from
Unity's `FieldInventory.cs`. The Unreal component is intentionally separate
from the renderer: adding it to a Blueprint does not create an inventory screen
or a fabricated physical terminal.

## What the component does

- Initializes a data-driven catalog of definitions and recipes. The built-in
  `InitializeFieldKit(WorldSeed, RifleReserve, SidearmReserve, InitialSpares)`
  creates the original five field items: rifle/sidearm ammunition, repair
  spares, recovered alloy and field samples. It starts with 18 alloy, a 32-slot
  backpack and 200 kg carry limit. The exact recipes are `ammo` (6 alloy → 30
  rifle rounds), `parts` (3 alloy → 4 spares), and `analyze` (1 sample → 4
  alloy). All of these are queryable with `Count` and `GetContainerItems`.
  The seed is 64-bit at the Blueprint boundary so source unsigned 32-bit world
  seeds above `2^31-1` remain intact.
- Maintains source-style item stacks, backpack/stash/loot containers, equipment
  slots, two-hand conflicts, stats, durability, consumable effects, cooldowns,
  lock and quest protections, hotbar, salvage, buying/selling and crafting.
  Mutations validate the whole state and roll back on failure, including full
  slots, overweight results and insufficient ingredients. `CanCraft` previews
  a recipe without spending anything.
- Exposes `CraftAtTerminal(RecipeId, Times, Context)`. Supply the **actual**
  city interaction state: on the operations floor within 3 m of the terminal,
  with Machineworks power at least `0.05`. A failed location/power check never
  spends resources. `Craft` is the ungated engine call for trusted game logic;
  player interaction should call `CraftAtTerminal`.
- Exposes a versioned `FSPInventorySnapshot`, `CreateSaveGame`,
  `RestoreSaveGame`, and `ValidateState`. The SaveGame includes item identity,
  stacks, slots, equipment, cooldowns, player resources, carry limit and
  revision. Restore requires the matching catalog to be initialized first;
  malformed saves are rejected atomically.

## Integration needed for the playable game

Instantiate the component on the authoritative player state or actor, then
initialize it from the existing combat reserve and ship-spares state **once**.
Afterward, personal weapon reloads call `Spend("rifle_ammo", rounds)` or
`Spend("sidearm_ammo", rounds)` and read `Count`; equipment repair calls
`Spend("spares", count)`. Existing independent ammo/spare fields must stop
writing their own separate copies. Successful surface collection calls
`AddItem("sample", collectedCount)` and emits the field-story sampler event.
The station/operations-room actor must construct `FSPFabricationContext` from
its actual proximity and Machineworks power value. A UMG/MFD inventory panel
can use `OnInventoryChanged`, `GetContainerItems`, `Count`, `CanCraft`, and
`CarriedWeightKg`.

This branch does **not** wire the component into the live level, combat,
vessel-systems spares, city machinery, MFD, or SaveGame manager. It does not
model the original engine's sort/move-to-index, random loot generation,
take-all or original JSON save interchange. The native catalog is supplied by
C++ or Blueprint structs; a cooked data-asset/catalog loader is a later
integration task. Until those bridges are built, the gameplay cannot claim a
working field inventory or fabrication station in the map.

## Validation

Build this worktree with Unreal Engine 5.8:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' SpacePatriotUnrealEditor Win64 Development '-Project=<this worktree>\SpacePatriotUnreal.uproject' -WaitMutex -NoHotReloadFromIDE
```

Run the isolated automation tests with a separate commandlet process:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' '<this worktree>\SpacePatriotUnreal.uproject' '-ExecCmds=Automation RunTests SpacePatriot.Inventory; Quit' -unattended -nop4 -nosplash -nullrhi -log
```

The field parity test checks the exact recipes, weights, location/power gate,
atomic overcapacity failure and SaveGame round trip. The generic engine test
checks stacks, locks, equipment conflicts/stats/durability, repair cost,
consumable cooldowns/hotbar, protected items, stash transfer and rollback when
a container is full.

Last validated with Unreal 5.8: build succeeded and both
`SpacePatriot.Inventory.FieldSourceParity` and
`SpacePatriot.Inventory.EngineSourceParity` returned `Result={Success}`.
