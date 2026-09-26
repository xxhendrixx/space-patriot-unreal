import json
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
worlds = json.loads((ROOT / "Data/Worlds.json").read_text(encoding="utf-8"))["worlds"]
creatures_root = json.loads((ROOT / "Data/CreatureRosters.json").read_text(encoding="utf-8"))
creatures = [entry for world in creatures_root["worlds"] for entry in world["fauna"]]
systems = (ROOT / "Source/SpacePatriotUnreal/Public/SpacePatriotSystemsComponent.h").read_text(encoding="utf-8")
bases = (ROOT / "Source/SpacePatriotUnreal/Public/SpacePatriotBlueprintBases.h").read_text(encoding="utf-8")

assert len(worlds) == 19, f"Expected 19 source worlds, found {len(worlds)}"
assert len(creatures) == 190, f"Expected 190 fauna records, found {len(creatures)}"
assert all(len(w["fauna"]) == 10 for w in creatures_root["worlds"]), "Each world must have ten fauna records"
assert all(len(item["concept"]["views"]) == 6 for item in creatures), "Creature concepts must have six standard views"
for name in ["AdvanceUniverse", "RollStoryEvent", "ApplyWildlifeDamage", "CalculateLoadedMass", "LoadSourceCatalogs"]:
    assert f"{name}(" in systems, f"Missing Blueprint system API: {name}"
for name in ["ASPWorldRuntime", "ASPPlayerShipPawn", "ASPWILDLIFEEncounter", "ASPCitizenAgent", "ASPCockpitMFD"]:
    if name == "ASPWILDLIFEEncounter":
        pattern = "ASPWildlifeEncounter"
    else:
        pattern = name
    assert pattern in bases, f"Missing Blueprint parent class: {pattern}"
assert "OnUniverseEvent" in systems and "OnHealthChanged" in bases, "Blueprint event dispatchers must be available"
print(f"PASS: {len(worlds)} worlds, {len(creatures)} fauna records, six-view concepts, and Blueprint hooks are present")
