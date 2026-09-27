"""Verify the Unreal Worldworks inputs against the original Unity engine export.

This checks identity, binary layout, seed, and biome coverage. It does not
claim that procedural rendering is visually identical to the original game.
"""

import hashlib
import json
import struct
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT.parent / "SpacePatriot" / "Assets" / "SpacePatriot" / "Resources" / "Worldworks"
DEST = PROJECT / "Data" / "Worldworks"
REPORT = PROJECT / "Data" / "WorldFieldAudit.json"


def inspect(path: Path, *, terrain: bool, expected_seed: int):
    data = path.read_bytes()
    width = struct.unpack_from("<i", data, 0)[0]
    height = width if terrain else struct.unpack_from("<i", data, 4)[0]
    size = struct.unpack_from("<f", data, 4)[0] if terrain else None
    seed = struct.unpack_from("<I", data, 8)[0]
    assert width >= 2 and height >= 2 and len(data) == 16 + width * height * 16, path
    assert seed == expected_seed, (path, seed, expected_seed)
    if terrain:
        assert size > 0, path
    return {
        "resolution": [width, height],
        "world_size_m": size,
        "seed": seed,
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "center_channels": [round(v, 6) for v in struct.unpack_from("<ffff", data, 16 + ((height // 2) * width + (width // 2)) * 16)],
    }


def main():
    worlds = json.loads((PROJECT / "Data" / "Worlds.json").read_text(encoding="utf-8"))["worlds"]
    original_available = SOURCE.is_dir()
    pinned = json.loads(REPORT.read_text(encoding="utf-8")) if REPORT.is_file() else None
    if not original_available and pinned is None:
        raise RuntimeError("Unity source or pinned WorldFieldAudit.json is needed for hash verification")
    result = {"world_count": len(worlds), "terrain": {}, "climate": {},
              "original_source_available": original_available, "hashes_verified": True}
    assert len(worlds) == 19
    for world in worlds:
        ident, seed, biome = world["id"], world["seed"], world["biome"]
        climate_rel = Path("Climate") / f"{ident}.bytes"
        target = DEST / climate_rel
        origin = SOURCE / climate_rel
        assert target.is_file(), climate_rel
        result["climate"][ident] = inspect(target, terrain=False, expected_seed=seed)
        if original_available:
            assert origin.is_file() and target.read_bytes() == origin.read_bytes(), climate_rel
        else:
            assert result["climate"][ident]["sha256"] == pinned["climate"][ident]["sha256"], climate_rel
        terrain_rel = Path(f"{ident}.bytes")
        target = DEST / terrain_rel
        origin = SOURCE / terrain_rel
        if biome == "gas":
            assert not target.exists(), terrain_rel
            continue
        assert target.is_file(), terrain_rel
        result["terrain"][ident] = inspect(target, terrain=True, expected_seed=seed)
        if original_available:
            assert origin.is_file() and target.read_bytes() == origin.read_bytes(), terrain_rel
        else:
            assert result["terrain"][ident]["sha256"] == pinned["terrain"][ident]["sha256"], terrain_rel
    assert len(result["terrain"]) == 13 and len(result["climate"]) == 19
    assert len({v["sha256"] for v in result["terrain"].values()}) == 13
    assert len({v["sha256"] for v in result["climate"].values()}) == 19
    REPORT.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"PASS: {len(result['terrain'])} original terrain fields; {len(result['climate'])} original climate atlases; all hashes match")


if __name__ == "__main__":
    main()
