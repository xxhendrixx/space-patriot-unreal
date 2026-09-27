# Optional free Fab ship and hangar packs

These are candidates for a local, separately licensed asset library. None of
their files are included in this public repository. Fab's [Standard License](https://www.fab.com/eula)
permits use in a game but does not grant a general right to redistribute raw
marketplace assets through a public Git repository. Fab's [Launcher guide](https://dev.epicgames.com/documentation/fab/exporting-assets-from-fab-in-launcher)
explains that Unreal-format products use **Create project** or **Add to project**
after acquisition; they do not have a direct UE file download option.

| Listing | Potential use | Status / caution |
| --- | --- | --- |
| [Arghanion's Flight System](https://www.fab.com/listings/62a69df5-b447-415d-8d91-82f9f1a46894) | UE 5.8 Blueprint project with walk-up boarding, takeoff, surface landing, cargo hold/doors, thrusters, and ship HUD. Strong reference for a walkable cargo-ship loop. | Advertised free; not yet acquired locally. Its modified Corinth Cargo Transporter ship is derived from [Yvo Pors's CC BY 4.0 model](https://sketchfab.com/3d-models/corinth-cargo-transporter-7614d85856f6477ba2a603970dcbb0dd); preserve attribution if used. Validate appearance and behavior in an isolated local project before migration. |
| [Neon Parallax](https://www.fab.com/listings/78245331-21d2-45c2-9f88-378c43890ff7) | Free modular sci-fi hangar with ships and showcase/overview maps. | Not yet acquired locally. Inspect the style and supported engine version before use in Kellen Reach. |
| [Spaceship Modular Star Sparrow](https://www.fab.com/listings/932ce79b-06e8-416a-ba0d-36435621a0a1) | Free modular fighter sample with 40 combinations and color instances. | Not yet acquired locally. Its publisher says the 2K sample is unsuitable for close camera shots; consider it for distant traffic rather than the player's walkable ship. |

The current source game can run without these packs. Keep any downloaded Fab
project in a non-Git local staging directory and record the listing URL and
installed engine version before migrating selected assets. Do not add its raw
`.uasset`, source textures, or meshes to the public repo.
