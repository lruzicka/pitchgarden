#!/usr/bin/env python3
"""Stage both Linux VST3 bundles, retain previous versions, then rename into place."""
import argparse
from datetime import datetime, timezone
import os
from pathlib import Path
import shutil
import tempfile
import uuid

repo = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build-dir", type=Path, default=repo / "build")
parser.add_argument("--destination", type=Path, default=Path.home() / ".vst3")
args = parser.parse_args()
destination = args.destination.expanduser().resolve()
bundles = [("PitchGardenARA", "PitchGarden"), ("PitchGardenMIDI", "PitchGarden MIDI")]
for target, name in bundles:
    source = args.build_dir / f"{target}_artefacts" / "Release/VST3" / f"{name}.vst3"
    if not list(source.glob("Contents/*-linux/*.so")):
        parser.error(f"Built Linux bundle missing: {source}; run scripts/build.sh first")
destination.mkdir(parents=True, exist_ok=True)
backup_root = destination.parent / ".pitchgarden-plugin-backups" / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S") + f"-{os.getpid()}-{uuid.uuid4().hex[:8]}")
with tempfile.TemporaryDirectory(prefix=".pitchgarden-install-", dir=destination.parent) as staging_name:
    staging = Path(staging_name)
    for target, name in bundles:
        shutil.copytree(args.build_dir / f"{target}_artefacts" / "Release/VST3" / f"{name}.vst3", staging / f"{name}.vst3")
    installed = []
    try:
        for _, name in bundles:
            target = destination / f"{name}.vst3"
            backup = None
            if target.exists() or target.is_symlink():
                backup_root.mkdir(parents=True, exist_ok=True)
                backup = backup_root / target.name
                target.rename(backup)
            try:
                (staging / target.name).rename(target)
            except Exception:
                if backup is not None:
                    backup.rename(target)
                raise
            installed.append((target, backup))
    except Exception:
        for target, backup in reversed(installed):
            target.rename(staging / target.name)
            if backup is not None:
                backup.rename(target)
        raise
for target, backup in installed:
    print(f"Installed: {target}")
    if backup:
        print(f"Previous bundle retained: {backup}")
print("Restart REAPER and rescan VST3 to load these builds; this installer does not stop REAPER.")
