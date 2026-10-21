# PitchGarden: Linux / REAPER ARA pitch editor (development scaffold)

**Status: 0.1, engineering bootstrap, NOT a usable pitch correction plugin yet.**

This repository starts with a working ARA integration *reference implementation* from JUCE,
compiled under our own VST3 plugin identity, plus a separately testable monophonic pitch estimator.
The ARA plugin initially displays the JUCE sample's audio timeline and plays the audio
unchanged (the sample contains a dimming operation). It does **not** display editable
pitch blobs or process corrected audio. This deliberate separation keeps the host bridge
verifiable before adding DSP.

## Fedora dependencies

```bash
sudo dnf install git gcc-c++ cmake ninja-build pkgconf-pkg-config \
  alsa-lib-devel freetype-devel fontconfig-devel libX11-devel libXext-devel \
  libXinerama-devel libXrandr-devel libXcursor-devel mesa-libGL-devel \
  libcurl-devel webkit2gtk4.1-devel
```

Packages can vary by Fedora release; consult JUCE Linux dependencies if configure fails.

## Step 1: local DSP tests (no SDK/network needed)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

## Step 2: ARA bootstrap plugin

```bash
./scripts/bootstrap.sh
./scripts/build.sh
mkdir -p ~/.vst3
cp -a "build/PitchGardenARA_artefacts/VST3/PitchGarden ARA (bootstrap).vst3" ~/.vst3/
```

**Note:** JUCE tag 8.0.10 and SDK branch `releases/2.3.0` are a pinned starting
configuration, but were not downloadable/compiled in the authoring environment.
If a tag is unavailable, select a published compatible JUCE release and preserve
that exact version in the project.

In REAPER Preferences > Plug-ins > ARA, enable ARA for supported VST3 plugins,
restart REAPER and rescan VST3. Add PitchGarden ARA to a take FX chain for testing.

## Architecture and next work

- `src/PitchDetector.*`: offline YIN-based pitch-frame extraction. No realtime allocations.
- `src/ARAEntry.cpp`: adapter that uses the upstream JUCE ARA demo. **Temporary bootstrap.**
- Next: replace the demo with a custom `ARADocumentControllerSpecialisation`,
  `ARAPlaybackRenderer`, and `AudioProcessorEditorARAExtension`.
- Analysis worker reads audio from `ARAAudioSourceReader` on a background thread.
- Note segmentation builds stable pitch blobs from voiced frames.
- The editor stores note-specific semitone/cent offsets and edits per audio modification.
- Playback renderer uses a pitch-shifting engine and crossfades at edit boundaries.
- Host project state is serialized using ARA document archive; use host undo integration.
- Handle source/host sample rate mismatch, channel count, loops, tempo changes,
  moved/split items, clones and playback-time mappings explicitly.

## Licensing

PitchGarden source may be distributed under GPLv3 subject to obligations of
JUCE, third-party processing libraries, and the bundled reference demo.
JUCE demo code has an ISC notice in the upstream file. Do not remove it.
Check ARA SDK and JUCE licensing before redistribution. No third-party source
is included inside this archive.
