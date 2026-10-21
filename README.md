# PitchGarden 0.2.0

Analysis-only monophonic pitch visualization for Linux ARA VST3 hosts. Recorded
ARA source samples feed the existing YIN detector on background workers. A piano
roll shows read-only note candidates and the original continuous pitch trajectory.
There is no pitch correction, transposition, or timing editing.

**Build and automated ARA integration tests pass on Fedora. Interactive REAPER
acceptance is NOT VERIFIED. Milestone 0.2's host acceptance gate remains open.**

## Dependencies and build

C++17, CMake 3.22+, a C++ compiler, and the existing pinned submodules are required.

```bash
sudo dnf install git gcc-c++ cmake ninja-build pkgconf-pkg-config \
  alsa-lib-devel freetype-devel fontconfig-devel libX11-devel libXext-devel \
  libXinerama-devel libXrandr-devel libXcursor-devel mesa-libGL-devel \
  libcurl-devel webkit2gtk4.1-devel
```

The actual JUCE gitlink is `454fe891acc8204f99f4ef1e3a28c84b1a4da145`
(**JUCE 9.0.4 plus one commit**), not JUCE 8 as claimed by the old scaffold README.
It has been preserved. JUCE 8 compatibility has **not** been tested. ARA SDK is
`a2b1aac1d1d5c4eed387db85a9c0cdb7d460254c` near release 2.3.0, with API
`65ec5c43b943a48cb5446f448a0492db6af8534b` and Library
`d18a6a5e489816316be84a9de0eaf7307bc1abe4`. No SDK sources were changed.

Initialize dependencies once (requires network only when objects are absent):

```bash
./scripts/bootstrap.sh
```

DSP tests work without JUCE, ARA, a GUI, or network:

```bash
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --parallel 4
ctest --test-dir build-core --output-on-failure
```

Full plugin and in-process ARA regression tests:

```bash
./scripts/build.sh
# Equivalent:
cmake -S . -B build -DPITCHGARDEN_BUILD_PLUGIN=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
mkdir -p ~/.vst3
cp -a build/PitchGardenARA_artefacts/Release/VST3/PitchGarden.vst3 ~/.vst3/
```

Ordinary configure/build never downloads dependencies. `JUCE_DIR` and
`ARA_SDK_DIR` can override source paths. `PITCHGARDEN_BUILD_PLUGIN_TESTS=OFF`
omits the SDK-dependent integration test. Plugin identity remains
`org.pitchgarden.pitchgarden`, manufacturer `PtGn`, plugin code `PgA1`.

## Editor

Add PitchGarden as an ARA effect to recorded audio in REAPER. The editor prefers
host-selected playback regions; when no region is selected it shows visible
region sequences. Source analysis is shared only between regions of the same
ARA source within its document. The source's sample rate determines timestamps.
Source offsets and current region timeline positions determine visualization.

- Wheel: scroll time. Shift + wheel: scroll pitch.
- Ctrl + wheel: zoom time around the cursor. Ctrl + Shift + wheel: zoom pitch.
- **Fit**: fit displayed regions and detected pitches.
- Click a note: select it and show continuous MIDI pitch, cents, frequency, and quality.
- Rectangles sit on the representative note row; the white curve retains the
  original pitch, including deviations above and below that row.

The status shows waiting, progress, ready/no stable notes, read errors, and
unsupported mappings. Results are in memory and recomputed after reopening a
project. The legacy demo archive format is still readable; legacy dim flags are
ignored. No editable note state is saved.

## Implementation

| Component | Responsibility |
| --- | --- |
| `PitchDetector.*` | Existing offline YIN estimator, with invalid-input guards |
| `PitchAnalysis.*` | Validated settings, continuous MIDI conversion, source timestamps, incremental reads |
| `ARAAnalysisBridge.*` | JUCE ARA source reader, source rate/channels, stereo averaging |
| `AnalysisModel.*` | Cancellable worker and immutable thread-safe result snapshots |
| `AnalyzedSource.h` | Source lifecycle, invalidation, reader ownership and reanalysis |
| `NoteSegmenter.*` | Independent conservative median-based note segmentation |
| `PitchGardenPlayback.h`, `PlaybackCache.h`, `DryPlayback.h` | Buffered dry rendering and region mapping |
| `PitchEditor.*` | Visible-range piano roll, curves, navigation and selection |
| `PitchGardenARA.h`, `ARAEntry.cpp` | Document controller, processor, editor binding and legacy archive compatibility |

Default analysis: 2048-sample window, 256-sample hop, 65–1200 Hz, YIN threshold
0.15. The window increases to at least `ceil(2 * sampleRate / 65) + 2` samples
when needed (notably at 96 kHz); this prevents truncating the lowest YIN lags.
Frames are timestamped at window centers. Incomplete trailing windows are omitted.
The stereo analysis signal is the arithmetic channel average; opposite-polarity
channels can cancel and are a known limitation.

Default segmentation: 70 ms minimum duration, 40 ms maximum bridged gap,
0.7 minimum confidence, 0.9-semitone transition threshold, 120 ms persistence,
and at most 0.5-semitone spread within a transition candidate. These are engineering
defaults, not universal singing rules. MIDI note labels are rounded for the grid;
continuous median pitch, cents, confidence, and original frame indices remain
available. Large vibrato or unusually slow slides may still need tuning.

Readers are created/destroyed within ARA source-management/endEditing callbacks.
Only synchronized source-reader sample access runs on an analysis worker.
Cancellation joins workers on the model thread before destroying readers or
sources, and clears stale published results. A host read already in progress
must finish before cancellation returns. Nothing analyzes, allocates recording
buffers, or waits for analysis on the realtime audio thread. Each source has an
analysis worker; many simultaneous long sources can consume substantial CPU.

Playback uses a separate buffering thread and preallocated render scratch space.
At equal rates, integral source samples pass through at unity, without demo
dimming. Mono is duplicated to stereo; stereo-to-mono output is averaged.
Different rates use **linear sample-rate conversion**, which is not a transparent
or band-limited converter. The source trajectory still uses the original rate.
Realtime cache reads use atomic lifetime gates and try-locks; host I/O runs on
the playback worker into staging buffers. Four preallocated windows retain distant
source offsets; more than four distant active regions can cause repeated fallback.
A cache miss preserves the complete host input block and reports
status; whether that input contains usable dry audio depends on the ARA host.
Offline rendering can wait for cached reads. Seek/dropout behavior requires the manual acceptance checks.

Stretch/rate transformations are not advertised to the host. If nevertheless
received, unsupported transformations, nonmatching region durations, and invalid
mapping coordinates produce an unavailable status; curves are withheld and host
input is retained. That fallback does not guarantee playback in every ARA host.
Regions outside the source's supported mapping cannot be silently displayed as
correct. The demo's loop audition is omitted because its rate mapping was not
robust. Normal host transport playback is supported by the dry renderer.

## Verification

Six standalone CTest tests cover detector reference frequencies/sample rates,
silence, bad input, conversions/time mapping, note segmentation, cancellation,
publication, shutdown, and unity sample conversion. A seventh optional test creates
an in-process ARA host and exercises real source-reader callbacks, source
analysis, region offsets, dry rendering, moves/trims, stereo 96 kHz replacement,
reader cleanup, nonblocking cache misses with blocked host I/O, and 96-to-48 kHz
rendering. It also writes `pitchgarden-piano-roll.png` in its working
directory for inspection. This is not REAPER compatibility certification.

Follow [the Fedora/REAPER acceptance procedure](docs/REAPER-ACCEPTANCE.md) before
calling the milestone complete. The implementation record is in
[IMPLEMENTATION-0.2.md](docs/IMPLEMENTATION-0.2.md).

## Licensing

Project license: GPLv3. The adapted integration retains the upstream JUCE demo's
ISC notice. The pinned JUCE 9 checkout offers commercial/AGPLv3 terms; review
those terms and the ARA SDK licenses before redistribution. No dependency license
compatibility claim is made here.
