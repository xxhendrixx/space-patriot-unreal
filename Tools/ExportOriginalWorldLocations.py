"""Export the original game's deterministic world and primary-port locations.

Run from any directory::

    python Tools/ExportOriginalWorldLocations.py
    python Tools/ExportOriginalWorldLocations.py --check

This is a small Python replay of the location math in the archived browser
sources: core.js hash/random/noise (lines 52-104), living.js Universe and site
(lines 398-514), and expedition.js exterior clearance (lines 842-850). It uses
the versioned Unreal Worlds/Settlements catalogs, so collaborators do not need
the complete archived browser source or Node. If the original Cosmoplot JSON is
available beside the Unreal repo, its order, seeds and radii are cross-checked.

Positions are *initial catalog* kilometres in the source game's X/Y-up/Z
frame. celestial.js subsequently moves worlds along time-varying orbits. These
large absolute coordinates must not be placed directly in an Unreal float
world: travel code should use a local origin/rebase per destination. This
export supplies stable IDs, sector membership, site orientation, and a default
arrival bearing, not an orbital simulator or a complete city layout.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORLD_FILE = ROOT / "Data" / "Worlds.json"
SETTLEMENT_FILE = ROOT / "Data" / "Settlements.json"
OUTPUT = ROOT / "Data" / "WorldLocations.json"
ORIGINAL_CATALOG = ROOT.parent / "SpacePatriot" / "Reference" / "Original" / "assets" / "data" / "cosmoplot-worlds.json"

SECTOR_KM = 50_000_000.0  # core.js SECTOR
NOISE_SIZE = 64
NOISE_SEED = 41827  # default base Universe seed, living.js constructor
MASK = 0xFFFFFFFF

# The exact living.js biomeByName entries for the 19 Cosmoplot worlds.
BIOME_INDEX = {
    "Mercury": 6, "Venus": 1, "Earth": 7, "Mars": 1,
    "Jupiter": 12, "Saturn": 12, "Uranus": 12, "Neptune": 12,
    "TRAPPIST-1 b": 3, "TRAPPIST-1 c": 1, "TRAPPIST-1 d": 1,
    "TRAPPIST-1 e": 7, "TRAPPIST-1 f": 10, "TRAPPIST-1 g": 10,
    "TRAPPIST-1 h": 6, "TOI-270 b": 1, "TOI-270 c": 12,
    "K2-141 b": 3, "WASP-76 b": 12,
}
# BIOMES entries used by those worlds: (terrain type, liquid flag).
BIOME_PROFILE = {1: (2, 1), 3: (5, 2), 6: (0, 0), 7: (1, 1), 10: (4, 1), 12: (3, 0)}
REDUCED_BIOME = {0: "rock", 1: "temperate", 2: "desert", 3: "gas", 4: "ice", 5: "volcanic"}


def u32(x: int) -> int:
    return x & MASK


def source_hash(x: int) -> int:
    """core.js hash, including JavaScript's 32-bit imul overflow."""
    x = u32(x)
    x = u32(x ^ (x >> 16))
    x = u32(x * 0x7FEB352D)
    x = u32(x ^ (x >> 15))
    x = u32(x * 0x846CA68B)
    return u32(x ^ (x >> 16))


class SourceRandom:
    def __init__(self, seed: int):
        self.state = u32(seed)

    def next(self) -> float:
        self.state = u32(self.state + 0x6D2B79F5)
        t = u32((self.state ^ (self.state >> 15)) * (self.state | 1))
        t = u32(t ^ u32(t + u32((t ^ (t >> 7)) * (t | 61))))
        return u32(t ^ (t >> 14)) / 4294967296.0


class SourceNoise:
    def __init__(self):
        self.data = bytes(source_hash(i ^ NOISE_SEED) >> 24 for i in range(NOISE_SIZE ** 3))

    def sample(self, x: float, y: float, z: float) -> float:
        ix, iy, iz = math.floor(x), math.floor(y), math.floor(z)
        fx, fy, fz = smooth(x - ix), smooth(y - iy), smooth(z - iz)

        def at(a: int, b: int, c: int) -> float:
            return self.data[(a & 63) + ((b & 63) << 6) + ((c & 63) << 12)] / 255.0

        def lerp(a: float, b: float, t: float) -> float:
            return a + (b - a) * t

        return lerp(
            lerp(lerp(at(ix, iy, iz), at(ix + 1, iy, iz), fx),
                 lerp(at(ix, iy + 1, iz), at(ix + 1, iy + 1, iz), fx), fy),
            lerp(lerp(at(ix, iy, iz + 1), at(ix + 1, iy, iz + 1), fx),
                 lerp(at(ix, iy + 1, iz + 1), at(ix + 1, iy + 1, iz + 1), fx), fy),
            fz,
        )


def smooth(x: float) -> float:
    return x * x * (3.0 - 2.0 * x)


def add(a: list[float], b: list[float]) -> list[float]:
    return [a[i] + b[i] for i in range(3)]


def mul(a: list[float], scale: float) -> list[float]:
    return [v * scale for v in a]


def unit(a: list[float]) -> list[float]:
    size = math.hypot(*a)
    if size <= 1e-14:
        return [0.0, 1.0, 0.0]
    return [v / size for v in a]


def cross(a: list[float], b: list[float]) -> list[float]:
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def raw_height(normal: list[float], body: dict, noise: SourceNoise) -> float:
    if body["type"] == 3:
        return 0.0
    ox, oy, oz = body["noise_offset"]

    def sample(scale: float) -> float:
        frequency = body["terrain_frequency"]
        return noise.sample(normal[0] * scale * frequency + ox,
                            normal[1] * scale * frequency + oy,
                            normal[2] * scale * frequency + oz)

    q = 2.0 * sample(12.0) - 1.0
    v = 0.58 * sample(3.5) + 0.26 * (1.0 - q * q) + 0.14 * sample(42.0) + 0.02 * sample(135.0)
    return body["radius_km"] * body["terrain_amplitude"] * (v - body["terrain_base"] - 0.02)


def site_for(body: dict, noise: SourceNoise) -> dict:
    rng = SourceRandom(body["seed"] ^ 23991)
    normal = unit([0.2 + rng.next() * 0.3, 0.65 + rng.next() * 0.18, 0.42 + rng.next() * 0.28])
    if body["liquid"] and body["type"] != 3:
        score = math.inf
        for _ in range(44):
            candidate = unit([0.15 + rng.next() * 0.7,
                              0.35 + rng.next() * 0.8,
                              0.15 + rng.next() * 0.7])
            candidate_score = abs(raw_height(candidate, body, noise) - 0.012)
            if candidate_score < score:
                score, normal = candidate_score, candidate
    height = (body["radius_km"] * body["atmosphere"] * 0.5 if body["type"] == 3
              else max(raw_height(normal, body, noise), 0.0)) + 0.022
    right = unit(cross([0.0, 1.0, 0.0], normal))
    forward = unit(cross(right, normal))
    center = add(body["center_km"], mul(normal, body["radius_km"] + height))
    return {
        "center_source_y_up_km": round_vector(center),
        "normal_source_y_up": round_vector(normal),
        "right_source_y_up": round_vector(right),
        "forward_source_y_up": round_vector(forward),
        "height_above_nominal_radius_km": round(height, 12),
    }


def round_vector(values: list[float]) -> list[float]:
    return [round(v, 12) for v in values]


def verify_original_catalog(worlds: list[dict]) -> None:
    if not ORIGINAL_CATALOG.exists():
        return
    original = json.loads(ORIGINAL_CATALOG.read_text(encoding="utf-8"))
    source_worlds = [(system["name"], planet) for system in original["systems"] for planet in system["planets"]]
    if len(source_worlds) != len(worlds):
        raise ValueError("Original Cosmoplot planet count differs from Data/Worlds.json")
    for world, (system, planet) in zip(worlds, source_worlds):
        if (world["id"], world["name"], world["system"], world["seed"]) != (
            planet["id"], planet["name"], system, planet["worldSeed"]
        ) or not math.isclose(world["radius"], planet["radiusEarth"], rel_tol=0.0, abs_tol=1e-12):
            raise ValueError(f"Source catalog/order mismatch at {world['id']}")


def build_export() -> dict:
    worlds = json.loads(WORLD_FILE.read_text(encoding="utf-8"))["worlds"]
    settlements = json.loads(SETTLEMENT_FILE.read_text(encoding="utf-8"))["settlements"]
    if len(worlds) != 19 or len({w["id"] for w in worlds}) != len(worlds):
        raise ValueError("Expected the original 19 unique world IDs")
    verify_original_catalog(worlds)
    primary_by_world: dict[str, dict] = {}
    for settlement in settlements:
        if not settlement["primary"]:
            continue
        world_id = settlement["world"]
        if world_id in primary_by_world:
            raise ValueError(f"Duplicate primary settlement for {world_id}")
        primary_by_world[world_id] = settlement
    if set(primary_by_world) != {w["id"] for w in worlds}:
        raise ValueError("Every world needs exactly one primary settlement")

    noise = SourceNoise()
    systems: list[str] = []
    per_system_index: dict[str, int] = {}
    locations = []
    for world in worlds:
        system = world["system"]
        if system not in systems:
            systems.append(system)
        system_index = systems.index(system)
        planet_index = per_system_index.get(system, 0)
        per_system_index[system] = planet_index + 1
        name, seed = world["name"], int(world["seed"])
        if name not in BIOME_INDEX or not 0 <= seed <= MASK:
            raise ValueError(f"Unknown source biome/seed for {name}")
        terrain_type, liquid = BIOME_PROFILE[BIOME_INDEX[name]]
        if world["biome"] != REDUCED_BIOME[terrain_type]:
            raise ValueError(f"World biome differs from living.js for {name}")
        if name in {"Venus", "Mars", "Mercury"}:
            liquid = 0  # living.js intentionally removes fictional oasis water here.
        rng = SourceRandom(seed)
        radius = max(160.0, min(3600.0, math.sqrt(float(world["radius"])) * 900.0))
        orbit = 7000.0 + planet_index * 14000.0
        angle = planet_index * 2.399
        star_center = [system_index * SECTOR_KM, 0.0, 0.0]
        center = add(star_center, [math.cos(angle) * orbit,
                                   (rng.next() - 0.5) * orbit * 0.05,
                                   math.sin(angle) * orbit])
        amplitude = 0.0 if terrain_type == 3 else 0.0025 + rng.next() * 0.002
        frequency = 0.75 + rng.next() * 0.75
        body = {
            "seed": seed,
            "type": terrain_type,
            "liquid": liquid,
            "center_km": center,
            "radius_km": radius,
            "atmosphere": 0.0 if terrain_type == 0 else (0.028 if terrain_type == 3 else 0.014),
            "terrain_amplitude": amplitude,
            "terrain_frequency": frequency,
            "terrain_base": 0.53 if terrain_type == 1 else 0.44,
            "noise_offset": [seed % 57, (seed >> 8) % 59, (seed >> 16) % 61],
        }
        primary = primary_by_world[world["id"]]
        source_id = "SP-" + world["id"]
        # expedition.js planetArrival guarantees this much exterior clearance.
        clearance = max(14.0, radius * body["atmosphere"] * 1.2 + 2.0,
                        radius * amplitude + 3.0)
        locations.append({
            "id": world["id"],
            "source_id": source_id,
            "name": name,
            "system": system,
            "system_index": system_index,
            "planet_index": planet_index,
            "sector_cell": [system_index, 0, 0],
            "system_origin_source_y_up_km": round_vector(star_center),
            "catalog_center_source_y_up_km": round_vector(center),
            "gameplay_radius_km": round(radius, 12),
            "physical_radius_km": round(float(world["radius"]) * 6371.0, 12),
            "seed": seed,
            "biome": world["biome"],
            "surface_landing_supported": terrain_type != 3,
            "minimum_exterior_clearance_km": round(clearance, 12),
            "primary_settlement_id": primary["id"],
            "primary_settlement_name": primary["name"],
            "primary_site": site_for(body, noise),
        })

    if len(systems) != 5 or sorted(per_system_index.values()) != [1, 1, 2, 7, 8]:
        raise ValueError("Unexpected system ordering or world distribution")
    for world in locations:
        site = world["primary_site"]
        if not math.isclose(math.hypot(*site["normal_source_y_up"]), 1.0, abs_tol=1e-9):
            raise ValueError(f"Invalid site normal for {world['id']}")
        if not math.isclose(math.dist(site["center_source_y_up_km"], world["catalog_center_source_y_up_km"]),
                            world["gameplay_radius_km"] + site["height_above_nominal_radius_km"],
                            abs_tol=1e-6):
            raise ValueError(f"Site center outside source radius for {world['id']}")
    return {
        "format": "space-patriot-original-world-locations",
        "version": 1,
        "units": "kilometers",
        "coordinate_frame": "original source X/Y-up/Z; initial catalog positions before celestial.js orbital advancement",
        "sector_size_km": SECTOR_KM,
        "source_algorithm": ["core.js:52-104", "living.js:398-514", "expedition.js:842-850"],
        "worlds": locations,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Verify the committed export matches a fresh replay")
    args = parser.parse_args()
    rendered = json.dumps(build_export(), indent=2, ensure_ascii=False) + "\n"
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != rendered:
            raise SystemExit("WorldLocations.json is missing or stale; rerun this exporter")
        print("WorldLocations.json matches all 19 original locations and primary sites")
    else:
        OUTPUT.write_text(rendered, encoding="utf-8")
        print(f"Exported 19 source world locations and primary sites to {OUTPUT}")


if __name__ == "__main__":
    main()
