#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p external
if [[ ! -d external/JUCE/.git ]]; then
  git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git external/JUCE
fi
if [[ ! -d external/ARA_SDK/.git ]]; then
  git clone --recursive --branch releases/2.2.0 https://github.com/Celemony/ARA_SDK.git external/ARA_SDK
fi
