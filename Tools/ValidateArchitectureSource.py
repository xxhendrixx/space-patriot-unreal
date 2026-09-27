"""Verify the copied ship-interior catalog against its Unity source contract."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "Data" / "Architecture" / "DeckPlans.json"
SOURCE = ROOT.parent / "SpacePatriot" / "Assets" / "SpacePatriot" / "Resources" / "DeckPlans.json"
EXPECTED_SHA256 = "50fb9a59323f504888b28658fcd4d1636406cd4eca8181890d51f1d949484482"


def main() -> None:
    raw = CATALOG.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == EXPECTED_SHA256, f"Deck catalog changed without a source parity review: {digest}"
    if SOURCE.is_file():
        assert raw == SOURCE.read_bytes(), "Unreal deck catalog diverges from the preserved Unity source"
    plans = json.loads(raw)["plans"]
    assert [p["family"] for p in plans] == list(range(10))
    assert [len(p["decks"]) for p in plans] == [1, 1, 2, 1, 1, 2, 2, 3, 1, 4]
    assert (len(plans[9]["rooms"]), len(plans[9]["doors"]), len(plans[9]["fixtures"])) == (53, 52, 176)
    for plan in plans:
        deck_indices = {d["index"] for d in plan["decks"]}
        assert deck_indices == set(range(len(plan["decks"])))
        ids = [item["id"] for key in ("rooms", "doors", "fixtures") for item in plan[key]]
        assert len(ids) == len(set(ids)), f"duplicate source ID in family {plan['family']}"
        assert all(item["deck"] in deck_indices for key in ("rooms", "doors", "fixtures") for item in plan[key])
        if len(deck_indices) > 1:
            assert {stop["deck"] for stop in plan["lifts"]} == deck_indices
            assert len({stop["id"] for stop in plan["lifts"]}) == 1
        for door in plan["doors"]:
            assert door["axis"] in {"x", "z"}
            assert 0.55 <= door["width"] <= 6
            assert any(
                room["deck"] == door["deck"]
                and room["x0"] - 0.5 <= door["x"] <= room["x1"] + 0.5
                and room["z0"] - 0.5 <= door["z"] <= room["z1"] + 0.5
                for room in plan["rooms"]
            ), f"door {door['id']} is detached from source rooms"
    print(f"ARCHITECTURE_SOURCE_PASS: {len(plans)} families; SHA256 {digest}; largest 53 rooms / 4 decks / 52 doors / 176 fixtures")


if __name__ == "__main__":
    main()
