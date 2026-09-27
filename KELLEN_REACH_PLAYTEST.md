# Kellen Reach in-Editor playtest

Open `SpacePatriotUnreal.uproject` in Unreal Engine 5.8 and load
`/Game/SpacePatriot/Maps/L_KellenReachWalk`. Choose **Play in Selected Viewport**.
The walk, ship, jump, destination surface, survey, and port interactions run in
this map; no separate play window or flight map is required.

Walk with **WASD** and mouse. The nearby rifle pickup, enemy patrols, and ammo
counter are Epic Shooter template mechanics placeholders. The template score
display and floating AI debug labels are hidden in the integrated play loop.
The ship's hatch is on its port side. Press **E** nearby to board. On foot near
a terminal, a persistent services panel shows prices, stock, hold use, credits,
freight, and campaign feedback. Press **F** for freight or a matching case
objective; **3** cycles goods, **4** buys, **5** loads, **6** unloads, and **7**
sells one unit. Press **H** to cycle available cases, **M** to open or advance
dialogue, and **1/2** for choices. Press **I** to open/close the case journal.

In the ship, press **N** until the destination is Mars, then press **R** to
start auto route. It launches, retracts gear, climbs, jumps, approaches, and
tries a safe landing. The NAV MFD's **DEST** and **AUTO ROUTE** buttons use the
same route state as N/R; **Tab** enables its pointer. If auto route cannot find
a safe landing site, it leaves the ship in manual flight. Press **R**, **X**,
or a movement key to cancel auto route. After landing, press **E** to exit,
**B** to scan near the survey site, **Y** to collect a sample, and **E** to
reboard. A valid sample advances a matching campaign sample objective.

Manual flight remains available: **Space** launches; **W/S/A/D** and
**Space/Ctrl** translate; arrows pitch/yaw; **Q/E** roll; **G** toggles gear;
**J** charges a jump after clearing the station, climbing above 2 km, and
retracting gear; **K** cancels the charge. On arrival at a solid world, descend
within 650 m and slow below 80 m/s, then press **L** over a broad, level
surface. **V** changes camera; **F1/F2** choose vessel presets, **F3** cycles
MFD pages, and **F4** dims the MFD. **I** opens the case journal while aboard.

The latest saved-map check passed 102/102, and the native automation suite
passed 33/33, including the physical travel loop, auto-route touchdown and
manual takeover, and sample-to-campaign progression. A live Selected Viewport
pass completed Earth→Mars by N/R, landed, exited, scanned, sampled, and
reboarded. This short route is playable; the destination surface and cockpit
still need substantial visual work. Most source cities, NPC routines, ship
interiors, wildlife, audio, and objective interactions are not yet playable.
The legacy `/Game/SpacePatriot/Maps/L_KestrelFlight` map remains for flight
regression checks, not as the main route into the game.

Run `Tools/InstallAllDependencies.ps1` with the Editor closed after cloning.
`Tools/InstallAllDependencies.ps1 -VerifyOnly` checks local assets after a
later `git pull`; the public repo does not include third-party pack binaries.
