#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -DPITCHGARDEN_BUILD_PLUGIN=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target PitchGardenARA_VST3 -j "$(nproc)"
echo "Plugin bundle: build/PitchGardenARA_artefacts/VST3/"
