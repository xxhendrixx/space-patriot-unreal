# Poly Haven environment starter set

The thirty-three source files are downloaded locally from the URLs in
`manifest.json` by `Tools/InstallLocalDependencies.ps1` and verified against
their MD5 checksums. The downloads and generated Unreal imports are ignored by
Git; only this credit file and the dependency manifest are versioned.

- [Hangar Concrete Floor](https://polyhaven.com/a/hangar_concrete_floor),
  Dimitrios Savva: 2K diffuse, DirectX normal, roughness, ambient occlusion.
  The source texture covers a 2 m square. The Unreal apron material projects
  it from world XY at its native 200 cm tile size, independent of mesh UVs.
- [Boulder 01](https://polyhaven.com/a/boulder_01), Rico Cilliers: 1K FBX
  model and 1K diffuse, DirectX normal, roughness maps. Its four source LOD
  FBXs are currently four separate Unreal meshes, not one switching LOD chain.
- [Plastic Crate 02](https://polyhaven.com/a/plastic_crate_02), Fabi_G: 1K FBX
  cargo prop with diffuse, DirectX normal, roughness, and opacity mask maps.
- [Rocks Ground 02](https://polyhaven.com/a/rocks_ground_02), Rob Tuytel:
  2 m mixed stone/soil terrain tile, with 2K color and 1K DirectX normal,
  roughness, and ambient occlusion.
- [Grass Ground](https://polyhaven.com/a/grass_ground), Charlotte Baglioni:
  2.51 m dry grass/soil terrain tile at the same map resolutions.
- [Red Sand](https://polyhaven.com/a/red_sand), Rohit Seervi: 3 m compacted red
  sand terrain tile at the same map resolutions.
- [Namaqualand Boulder 03](https://polyhaven.com/a/namaqualand_boulder_03),
  Dario Barresi and Jenelle van Heerden: 3.1 m rugged brown boulder with
  1K FBX/PBR maps. The imported FBX currently has one Unreal LOD.
- [Namaqualand Boulder 05](https://polyhaven.com/a/namaqualand_boulder_05),
  Dario Barresi and Jenelle van Heerden: 1.4 m low weathered boulder with
  1K FBX/PBR maps. The imported FBX currently has one Unreal LOD.

License: [Poly Haven CC0](https://polyhaven.com/license). Poly Haven permits
commercial use, modification, and redistribution, including public GitHub
repositories. This license applies to these assets, not to other Fab content.

The associated Unreal importers are `Tools/ImportPolyHaven.py`,
`Tools/ImportPolyHavenTerrain.py`, and `Tools/ImportPolyHavenRocks.py`.
`Tools/GeneratePolyHavenRockLODs.py` is prepared for a later performance pass;
it has not been run yet.
Run `Tools/InstallLocalDependencies.ps1` to restore or verify the downloads
and Unreal imports. `Tools/StagePolyHaven.ps1` can refresh the upstream file
list when deliberately updating this asset set.
