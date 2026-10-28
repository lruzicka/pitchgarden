#!/usr/bin/env python3
"""Check generated VST3/ARA identities without loading or changing REAPER."""
import json
import pathlib
import re
import sys
build = pathlib.Path(sys.argv[1])
config = sys.argv[2] if len(sys.argv) > 2 else "Release"
identities = []
for target, name in (("PitchGardenARA", "PitchGarden"), ("PitchGardenMIDI", "PitchGarden MIDI")):
    root = build / f"{target}_artefacts" / config / "VST3" / f"{name}.vst3"
    # Steinberg's manifest writer emits trailing commas in the pinned SDK.
    data = json.loads(re.sub(r",\s*([}\]])", r"\1", (root / "Contents/Resources/moduleinfo.json").read_text()))
    assert data["Name"] == name
    ids = {entry["CID"] for entry in data["Classes"]}
    assert len(ids) == 3, (target, ids)
    flags = (build / "CMakeFiles" / f"{target}.dir/flags.make").read_text()
    properties = [re.search(key + r"=([^ ]+)", flags).group(1) for key in
                  ("JucePlugin_ARAFactoryID", "JucePlugin_ARADocumentArchiveID")]
    identities.append((ids, properties))
assert not identities[0][0] & identities[1][0], "VST3 classes overlap"
assert all(a != b for a, b in zip(identities[0][1], identities[1][1])), "ARA identities overlap"
print("Both built bundles have independent audio/controller/ARA VST3 classes, ARA factories and archives")
