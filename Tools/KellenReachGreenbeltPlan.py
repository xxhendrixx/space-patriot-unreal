"""Pure, deterministic layout for a restrained greenbelt on Kellen Reach's port edge.

Coordinates are Unreal centimetres. The plan is intentionally bound to the
existing collision Floor and avoids the central ship, cargo, and AI lanes.
"""

import random


PREFIX = "SP Greenbelt / "
SEED = 7260917
EXPECTED_COUNT = 28
MAX_COUNT = 40

# Conservative reserved rectangles, including room for foliage footprints.
# The inner lane includes the parked ship, boarding point, and on-foot start.
RESERVED = {
    "ship boarding lane": (-5000, 2000, -2400, 2400),
    "north cargo and AI route": (-7700, 6100, 2400, 5700),
    "south cargo and AI route": (-7700, 6100, -5700, -2400),
}


def intersects(a, b):
    """True if two (xmin, xmax, ymin, ymax) rectangles overlap."""
    return a[1] >= b[0] and a[0] <= b[1] and a[3] >= b[2] and a[2] <= b[3]


def approximate_footprint(item):
    # Larger than the scaled source bounds, and rotation-independent.
    radius = 330.0 if item["mesh"] == "shrub" else 190.0
    return (item["x"] - radius, item["x"] + radius,
            item["y"] - radius, item["y"] + radius)


def make_plan(floor_center_x, floor_center_y, floor_half_x, floor_half_y):
    """Return 28 stable placements for a floor at least 280 x 190 metres."""
    if floor_half_x < 14000 or floor_half_y < 9500:
        raise ValueError("Port collision floor is too small for the greenbelt")
    rng = random.Random(SEED)
    placements = []

    # Five paired clumps on each long edge. The y band starts past the rock
    # verge (~6,200-7,000 cm), leaving industrial circulation open.
    for side, sign in (("north", 1), ("south", -1)):
        for cluster in range(5):
            base_x = floor_center_x + (cluster - 2) * floor_half_x * 0.36 + rng.uniform(-250, 250)
            base_y = floor_center_y + sign * (floor_half_y - 1250 + rng.uniform(-130, 130))
            for plant in range(2):
                mesh = "rooibos" if (cluster + plant + (sign > 0)) % 4 == 0 else "shrub"
                placements.append({
                    "label": f"{PREFIX}{side} edge / clump {cluster + 1:02d} / plant {plant + 1:02d}",
                    "mesh": mesh,
                    "x": round(base_x + (-155 if plant == 0 else 175) + rng.uniform(-70, 70), 3),
                    "y": round(base_y + rng.uniform(-105, 105), 3),
                    "scale": round(rng.uniform(0.55, 0.77) if mesh == "shrub" else rng.uniform(0.72, 1.04), 4),
                    "yaw": round(rng.uniform(0, 360), 3),
                })

    # Two smaller clumps on each short edge complete the greenbelt without
    # filling the player's long sightline over the launch apron.
    for side, sign in (("east", 1), ("west", -1)):
        for cluster, y_fraction in enumerate((-0.47, 0.47)):
            base_x = floor_center_x + sign * (floor_half_x - 1280 + rng.uniform(-100, 100))
            base_y = floor_center_y + floor_half_y * y_fraction + rng.uniform(-200, 200)
            for plant in range(2):
                mesh = "rooibos" if plant == cluster else "shrub"
                placements.append({
                    "label": f"{PREFIX}{side} edge / clump {cluster + 1:02d} / plant {plant + 1:02d}",
                    "mesh": mesh,
                    "x": round(base_x + rng.uniform(-90, 90), 3),
                    "y": round(base_y + (-150 if plant == 0 else 170) + rng.uniform(-60, 60), 3),
                    "scale": round(rng.uniform(0.52, 0.72) if mesh == "shrub" else rng.uniform(0.68, 0.94), 4),
                    "yaw": round(rng.uniform(0, 360), 3),
                })

    if len(placements) != EXPECTED_COUNT or len(placements) > MAX_COUNT:
        raise AssertionError("Greenbelt actor budget changed")
    labels = {item["label"] for item in placements}
    if len(labels) != len(placements):
        raise AssertionError("Greenbelt labels are not unique")
    floor_rect = (floor_center_x - floor_half_x, floor_center_x + floor_half_x,
                  floor_center_y - floor_half_y, floor_center_y + floor_half_y)
    for item in placements:
        rect = approximate_footprint(item)
        if rect[0] < floor_rect[0] or rect[1] > floor_rect[1] or rect[2] < floor_rect[2] or rect[3] > floor_rect[3]:
            raise AssertionError("Plant escaped collision floor: " + item["label"])
        for zone_name, zone in RESERVED.items():
            if intersects(rect, zone):
                raise AssertionError("Plant entered {}: {}".format(zone_name, item["label"]))
    return placements
