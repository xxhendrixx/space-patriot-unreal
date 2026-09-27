# Original Space Patriot art from pack studies

This is the art and implementation handoff for the integrated Unreal project.
Free packs can help the team learn Unreal techniques and fill temporary visual
slots while the game becomes playable. The production target is a coherent
Space Patriot world built from the game's original systems, concept art and
authored assets. A free pack is not automatically original art, and a modified
pack remains an adapted third-party asset.

## Keep three kinds of work distinct

| Status | Meaning | In public source and final game |
| --- | --- | --- |
| `study` | A locally installed Fab project or pack inspected for broad techniques: how it organizes materials, modules, LODs, streaming, animation or Blueprint interfaces. | Store only our observations and independently written specifications in Git. Keep the pack out of the public repository. |
| `licensed-runtime` | A selected pack mesh, texture, animation or Blueprint actually used as a temporary or final game dependency. Recoloring, retopology or kitbashing it is still an adaptation. | Record its exact listing, acquired license and attribution conditions. Install it locally or distribute it only in a way that license allows. Keep its identity in the provenance record. |
| `original` | A new asset made from Space Patriot's own brief/concepts or an authorized artist's work, with independently authored topology, UVs, materials and animations. A generated draft still needs substantial cleanup and review. | Commit editable source, exports, and proof renders once the contributor confirms the rights and visual gate. |

The [Fab Standard License](https://www.fab.com/eula) allows use and modification
of acquired content in a project and sharing it with project collaborators, but
does not allow standalone redistribution. Some listings use CC BY or legacy
Marketplace terms instead; check the **specific acquired listing**, not just
its zero price. Do not describe a recolored or re-exported Fab mesh as our own
asset. Consult the listing's AI-use restrictions before feeding its images or
files to a generative model. Our own concept boards are the default input to
the local mesh workflow.

## Study a pack, then write our own requirement

Inspect the free candidates named in
[UNREAL_FULL_PORT_AND_ASSET_PLAN.md](UNREAL_FULL_PORT_AND_ASSET_PLAN.md) in a
local study project. Record a small technique note, not screenshots of whole
Blueprint graphs or copied functions. For each item, write: what problem it
solves, the public-facing behavior, data inputs/outputs, performance cost,
dependencies, Unreal version, and where it fails our requirements. Then design
the Space Patriot implementation from that note and the original source game.
Pack Blueprint code or graph translation is **not** the definition of an
independent implementation.

| Pack to inspect | Learn from it | Space Patriot implementation to own |
| --- | --- | --- |
| PlanetX and the free orbital samples | Camera-relative orbital presentation, atmosphere/cloud layering, transition budgets. | Original 19-body catalog, source seeds, world clock, body-local streaming, landing collision, climate and settlement placement. PlanetX itself says runtime World Partition ground entry is unsupported. |
| Starfield FREE | Scalable background layers and texture cost. | Actual system star positions, lighting, navigation markers and the game's own restrained sky treatment. |
| City Sample Buildings/Crowds | Modular dimensions, PCG dressing, far-crowd rendering. | Original 420 destination IDs, climate/industry architecture grammar, walkable interiors, NPC schedules and persistent events. |
| Modular SciFi packs | Grid/pivot standards, trim sheets, room assembly. | Hull-specific connected deck plans, doors, lifts, stations, usable cockpit controls and visual language. |
| Free ship and creature candidates | Moving-part splits, rig/socket conventions, LOD and collision strategies. | Ship families and fauna from Space Patriot concepts and gameplay data; matched hitboxes, animations and abilities. |

The game systems always own identity and behavior. A planet material must not
decide world radius or geology; a city mesh must not decide which settlement
exists; a creature model must not own its species AI; a ship mesh must not own
its flight controls. Stable IDs and semantic art slots connect authored
presentation to those systems.

## Production path for one original asset

1. **Choose a stable ID and functional brief.** Link the source record: a
   world/biome in `Data/Worlds.json`, a creature in
   `Data/CreatureRosters.json`, a room family in
   `Data/Architecture/DeckPlans.json`, or a ship role from the original source.
   Specify dimensions, moving parts, sockets, collision and gameplay role.
2. **Approve the concept and turnaround.** Use our own concept boards and six
   separate orthographic views where the form matters. Agree on camera scale,
   orientation, centerline, ground contact and palette. A three-quarter image
   is a review view, not a UV atlas. The original Unity art guide is
   `../SpacePatriot/ArtDirection/PIPELINE_STANDARD.md`.
3. **Make a geometry draft.** Hand-model in Blender or use the locally pinned
   TripoSG tool on our authorized concept images for individual parts. Do not
   run a whole ship, multideck interior or city through one mesh generator.
   Keep hatches, landing gear, engines, doors and MFD controls as separate
   objects with deliberate pivots. Record model/tool version, image hashes and
   seed when a generator is used.
4. **Author production geometry.** Fix proportions and silhouette in Blender;
   retopologize; make real openings and hard-surface detail, not painted
   imitations. Build clean UV0 islands and texture sets, LODs, collision, and
   named sockets. Bake material maps after the final UV layout, reopen the
   saved `.blend` and verify the packed/exported images. Re-bake tangent-space
   normals if the UV layout changes.
5. **Convert basis deliberately.** The older Kestrel authoring recipe is +X
   nose, +Y up, +Z starboard. `Tools/ConvertKestrelForUnreal.py` applies +90°
   around X, giving Unreal +X nose, +Z up, **−Y starboard**. Use the same
   documented transform for every imported Kestrel part and verify front,
   side and top after import. Never repair an axis error by mirroring a texture.
6. **Compare before integration.** Capture all six matched Blender and Unreal
   views beside the approved concept. Use the existing Kestrel contour/H/S
   comparison approach (magenta reference, cyan mesh, white overlap) and a
   neutral-material geometry pass. Metrics such as silhouette overlap and
   centroid expose errors, but an artist must inspect construction and style.
7. **Test function in the integrated map.** A ship needs symmetric hull/wing
   placement, correctly attached engines, working gear/hatch, collision,
   cockpit sightlines, LODs and boarding; an interior needs connected rooms
   and controls; a planet/biome asset needs correct scale, material transitions
   and terrain collision. Capture in-game views and only then replace the art
   registry slot. Keep the previous asset as a rollback until the packaged
   build passes.

For planetary art, build a *family* of original terrain materials, strata,
cliffs, flora and structures driven by the game's deterministic geology and
climate data. Existing Poly Haven CC0 materials can be local placeholders or
inputs with recorded provenance. The planet's shape, cell coordinates,
collision, weather and biome rules remain ours. One orbital sphere texture
cannot stand in for the landable planet.

## Visual and provenance gate

Each contribution folder should contain an editable `.blend`, export, source
textures, concept/turnaround links, six-view render board, Unreal in-game
captures, a review note and `provenance.json`. Keep Marketplace downloads in
the local dependency library; keep our accepted art in version control if its
rights and size permit. The minimum provenance record is:

```json
{
  "asset_id": "ship.kestrel.hull.v2",
  "status": "original",
  "creator": "contributor name",
  "concept_sources": ["../SpacePatriot/ArtDirection/Modules/kestrel-hull-ortho-v1/"],
  "source_game_record": "Kestrel ship role and dimensions",
  "study_packs": [{"listing_url": "https://www.fab.com/listings/...", "observed_technique": "pivot and moving-part organization"}],
  "included_third_party_files": [],
  "derived_from_pack": false,
  "generator": {"name": "none", "version": "", "seed": null, "input_hashes": []},
  "authoring": {"blender_file": "model.blend", "export": "export/asset.fbx", "unit": "metre", "forward": "+X", "up": "+Z"},
  "review": {"six_views": "renders/six-view.png", "in_game": "renders/unreal.png", "uv_check": "renders/uv.png", "approved_by": ""}
}
```

If `derived_from_pack` is true, change `status` to `licensed-runtime`, name
the source files and acquired license, and retain that classification through
export. If a study pack informed only a general technique, keep that URL in
`study_packs` but leave `included_third_party_files` empty. A clean manifest
helps collaborators reproduce the build and lets us measure how much of the
final game art is truly original.

## First art slice to prove the method

Take one location and route that the existing game can reach: Kellen Reach
apron → Kestrel cockpit → orbit → one other body. Create one original ship hull
module, one usable interior bay and one planet biome material/rock set.
Compare each in the integrated map under the same cameras used for concept
review. Accept only after the functional and visual gates pass; then expand
the same family rules to the other craft and worlds. This builds a recognizable
game while the full 19-world systems port continues.
