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

# Exercise the actual Linux companion factories with both DSOs loaded together.
# This verifies loader coexistence, not live REAPER loading/rendering.
if sys.platform.startswith("linux"):
    import ctypes as c
    class ClassInfo(c.Structure):
        _fields_ = [("cid", c.c_ubyte * 16), ("cardinality", c.c_int32),
                    ("category", c.c_char * 32), ("name", c.c_char * 64)]
    class ARAFactoryPrefix(c.Structure):
        _fields_ = [("size", c.c_size_t), ("lowest", c.c_int32), ("highest", c.c_int32), ("factory_id", c.c_char_p)]
    def method(obj, index, result, *args):
        table = c.cast(obj, c.POINTER(c.POINTER(c.c_void_p))).contents
        return c.CFUNCTYPE(result, c.c_void_p, *args)(table[index])
    loaded = []
    factory_ids = []
    for target, name in (("PitchGardenARA", "PitchGarden"), ("PitchGardenMIDI", "PitchGarden MIDI")):
        bundle = build / f"{target}_artefacts" / config / "VST3" / f"{name}.vst3"
        library = c.CDLL(str(next((bundle / "Contents").glob("*-linux/*.so")).resolve()), mode=c.RTLD_LOCAL)
        library.ModuleEntry.argtypes = [c.c_void_p]
        library.ModuleEntry.restype = c.c_bool
        assert library.ModuleEntry(library._handle)
        library.GetPluginFactory.restype = c.c_void_p
        factory = library.GetPluginFactory()
        loaded.append((library, factory))
        count = method(factory, 4, c.c_int32)(factory)
        assert count == 3
        for index in range(count):
            info = ClassInfo()
            assert method(factory, 5, c.c_int32, c.c_int32, c.POINTER(ClassInfo))(factory, index, c.byref(info)) == 0
            assert info.name.decode() == name
            if info.category == b"ARA Main Factory Class":
                cid = (c.c_ubyte * 16).from_buffer_copy(info.cid)
                iid = (c.c_ubyte * 16).from_buffer_copy(bytes.fromhex("DB2A1669FAFD42A5A82F864F7B6872EA"))
                main = c.c_void_p()
                assert method(factory, 6, c.c_int32, c.c_void_p, c.c_void_p, c.POINTER(c.c_void_p))(factory, cid, iid, c.byref(main)) == 0
                ara = method(main, 3, c.POINTER(ARAFactoryPrefix))(main)
                factory_ids.append(ara.contents.factory_id)
                method(main, 2, c.c_uint32)(main)
    assert len(factory_ids) == 2 and factory_ids[0] != factory_ids[1], factory_ids
    for library, factory in reversed(loaded):
        method(factory, 2, c.c_uint32)(factory)
        library.ModuleExit.restype = c.c_bool
        assert library.ModuleExit()
    print("Both Linux VST3 DSOs loaded simultaneously and returned distinct live ARA factories")
