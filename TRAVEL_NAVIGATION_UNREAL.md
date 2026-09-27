# Native travel state-machine slice

`USPTravelNavigationComponent` is a Blueprint-spawnable C++ component. It loads
the 19 IDs, names, systems, and biomes from `Data/Worlds.json`. It preserves the
separate pilot actions in the original `expedition.js` and `systems.js`:

- `SelectDestination`/`CycleDestination`, SCM/NAV, and the three-second cruise
  spool with 1.2-second release;
- a two-second sector charge, flight/gear/power/altitude (including the source
  body's radius-scaled minimum)/station/heat/fuel
  interlocks, a seven-point fuel debit at transit start, and an explicit
  exterior arrival at the selected world;
- a surface landing request below 80 m/s and within 300 m of a safe footprint,
  with touchdown confirmed by the physical ship actor; station docking is a
  separate action after landing at a station pad;
- a validated `FSPTravelNavigationState` save payload, including mid-spool and
  paid mid-transit states. An invalid restore leaves the current state intact.

`FSPTravelContext` must be populated from the live flight pawn, terrain/station
collision traces, and hatch controls each frame. `SetDriveResources` must be
kept in sync with the ship's fuel and heat. When `OnFuelChanged` fires, the
ship's resource owner must accept that debit. The route actor must move the
ship and call `ConfirmJumpArrival` only at an exterior arrival marker. On
`OnWorldArrived`, the world runtime should activate the matching world ID and
stream its terrain, NPCs, and encounters. For landing, call
`RequestSurfaceLanding` after the broad/dry footprint check and
`ConfirmSurfaceTouchdown` only when the ship's collision and gear animation
actually settle. `DockAtStation` requires `bAtStationPad` and `StationId`.

No route geometry, orbital motion, atmosphere crossing, cockpit UI, station
service menu, or physical landing is claimed here. The original
`celestial.js` rotates bodies and remaps routes as time advances; that spatial
system still needs an Unreal implementation. The Unity `FrontierGame.cs` jump
fade/teleport is also not reproduced; this slice requires an explicit spatial
route confirmation and avoids changing worlds before that confirmation.

Automation tests: `SpacePatriot.Travel` (five native Editor tests). The
baseline UE5.8 target currently has unrelated unity-build collisions between
anonymous-namespace helpers in `SPArchitecture.cpp`,
`SPStoryCampaignComponent.cpp`, and `SPWorldSurface.cpp`. The slice was built
and tested in a temporary isolated non-unity validation target; that target is
not part of this change.

## Hyperdrive visuals

`USPHyperdriveVisualComponent` reads `USPTravelNavigationComponent` every
frame. During `JumpCharging` it constructs converging cyan/amber alignment
brackets; during `JumpTransit` it constructs 72 restrained radial star streaks
on a camera-local plane. Cancel, exterior arrival, disabling speed effects,
and leaving those phases immediately clear the mesh. The vertex-color material
`M_HyperdriveVisual.uasset` is unlit and additive; no Niagara system, texture
atlas, camera FOV zoom, or full-screen white flash is used. The mesh has no
collision or shadow casting.

Add both components to the active flight Pawn and feed `AdvanceDrive` from its
pilot controls. The visual component auto-finds the travel component and the
Pawn's `UCameraComponent` on BeginPlay; Blueprint can also call
`SetNavigationComponent` and `BindToCamera` explicitly. The route actor remains
responsible for the actual ship trajectory and `ConfirmJumpArrival`. The
visual does not move the ship or change worlds. The saved material is generated
by `Tools/CreateHyperdriveVisualMaterial.py` for repeatable edits.

`SpacePatriot.Travel` now also tests both visual phases, charging radius
convergence, cancel/arrival/accessibility cleanup, cooked material loading,
camera attachment, and actual procedural mesh section upload.
