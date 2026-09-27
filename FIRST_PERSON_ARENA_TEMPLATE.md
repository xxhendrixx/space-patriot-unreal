# Local UE 5.8 shooter placeholders

The Kellen Reach on-foot map uses Epic's First Person Arena Shooter example for
its temporary character, rifle, NPC, controller, input actions and HUD. These
assets are **not stored in this Git repository**. A collaborator with Unreal
Engine 5.8 installed runs `Tools/InstallLocalDependencies.ps1` before opening
the map. `Data/ExternalRuntimeContentManifest.json` lists the exact 145
required template `.uasset` files, original engine-relative paths, sizes and
SHA-256 checksums. The script copies them into their `/Game` package paths and
verifies each copy. It does not acquire optional Fab marketplace packs.

The template's installed sources are under `Engine/Templates/TP_FirstPersonBP`
and `Engine/Templates/TemplateResources/{Standard,High}`. The dependency
closure for the two Space Patriot playtest maps uses approximately 79 MiB of
the example, compared with 150 MiB in the full Arena template. The stock
example maps and their external actors are outside this closure. The project
uses its own `L_KellenReachWalk` and `L_KestrelFlight` maps.

The shooter Blueprints require the `GameplayStateTree` and `EnhancedInput`
plugins plus the `Projectile` collision profile in `Config/DefaultEngine.ini`.
Those settings are versioned in the project. The current on-foot GameMode
references `BP_ShooterCharacter`, `BP_ShooterPlayerController` and
`UI_Shooter`; its placed pickup selects the `DT_WeaponList` `Rifle` row.

The [Unreal Engine EULA](https://www.unrealengine.com/eula/unreal) permits
distribution of installed template **Examples**, but keeping them as local
dependencies makes updates smaller and avoids uploading content everyone with
the required engine already has. This does not grant permission to redistribute
separate Fab marketplace packs.
