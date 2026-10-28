#!/usr/bin/env python3
"""Generate deterministic mono five-note/pauses fixtures for REAPER acceptance."""
import argparse
import math
from pathlib import Path
import struct
import wave
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "build/midi-fixtures")
parser.add_argument("--prefix-seconds", type=float, default=0, help="Leading silence for testing trimmed source offsets (0..30)")
args = parser.parse_args()
if not math.isfinite(args.prefix_seconds) or not 0 <= args.prefix_seconds <= 30:
    parser.error("prefix must be between 0 and 30 seconds")
args.output.mkdir(parents=True, exist_ok=True)
for name, pitches in (("bass", [28, 33, 38, 43, 47]), ("guitar", [45, 50, 55, 59, 64])):
    for rate in (44100, 48000):
        data = bytearray()
        for i in range(round(rate * (2 + args.prefix_seconds))):
            time = i / rate - args.prefix_seconds
            index = max(0, min(4, int(time / .4)))
            hz = 440 * 2 ** ((pitches[index] - 69) / 12)
            value = .6 * math.sin(2 * math.pi * hz * time) if time >= 0 and time - index * .4 < .3 else 0
            data.extend(struct.pack("<h", round(value * 32767)))
        path = args.output / f"five-{name}-notes-{rate}.wav"
        with wave.open(str(path), "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(rate)
            output.writeframes(data)
        print(path)
print(f"Each fixture: {args.prefix_seconds} seconds of leading silence, then five 0.3-second tones spaced 0.4 seconds apart.")
print("Bass MIDI pitches: 28,33,38,43,47. Guitar MIDI pitches: 45,50,55,59,64.")
