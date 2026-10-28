#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Restore exact gitlinks. Never select a different tag or update to remote HEAD.
git submodule update --init external/JUCE external/ARA_SDK
git -C external/ARA_SDK submodule update --init ARA_API ARA_Library
