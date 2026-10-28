#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -DPITCHGARDEN_BUILD_PLUGIN=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "${PITCHGARDEN_JOBS:-4}"
ctest --test-dir build --output-on-failure
echo "Plugin bundles:"
echo "  build/PitchGardenARA_artefacts/Release/VST3/PitchGarden.vst3"
echo "  build/PitchGardenMIDI_artefacts/Release/VST3/PitchGarden MIDI.vst3"
