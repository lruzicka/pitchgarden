# PitchGarden 0.2.1

Analysis-only monophonic pitch visualization for Linux ARA VST3 hosts. Recorded
ARA source samples feed the existing YIN detector on background workers. A piano
roll shows read-only note candidates and the original continuous pitch trajectory.
There is no pitch correction, transposition, or timing editing. Version 0.2.1 adds
transport visualization, optional Follow, host-compatible Space handling,
optional ARA original-note audition, and pitch-uncertainty annotations.

**Build and automated ARA integration tests pass on Fedora. Interactive REAPER
acceptance of the new 0.2.1 features is NOT VERIFIED. The live host acceptance
gate remains open. The user reports the existing 0.2 note display/navigation work.**

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
- **Follow**: keep the playback cursor visible while REAPER plays. Defaults off;
  wheel scrolling/zoom and Fit suspend it. Re-enable explicitly to resume.
- The yellow playback cursor and orange stopped cursor use REAPER project seconds,
  separately from source-relative pitch data. Seeking/loop jumps use new host
  positions directly. No position is invented when host callbacks stop.
- **Audition**: play the selected note's original source interval once through the
  optional ARA editor renderer, while host playback is stopped. **Stop preview**
  stops it. The preview does not seek REAPER, start track playback, change loop
  ranges, or modify the track renderer. The visible item clips the note interval.
- Space is left available to host keyboard routing. If Space reaches the editor
  and an ARA playback controller is provided, the editor requests host play/stop.
  Repeated key-downs are suppressed. Controls do not grab keyboard focus.
- Click a note: select it and show continuous MIDI pitch, cents, frequency, and quality.
- Rectangles sit on the representative note row; the white curve retains the
  original pitch, including deviations above and below that row. Orange dots
  indicate brief octave-like uncertainty; grey dots indicate low confidence.
  Raw detected values remain unchanged.

The status shows waiting, progress, ready/no stable notes, read errors, and
unsupported mappings. Results are in memory and recomputed after reopening a
project. The legacy demo archive format is still readable; legacy dim flags are
ignored. No editable note state is saved.

## Transport, keyboard and audition limitations

Transport is published from JUCE's audio callback, using finite project seconds
or sample-position/sample-rate fallback. Playback also accepts seconds-only
contexts rather than assuming timeline zero. The GUI never calls AudioPlayHead from
its timer and does not extrapolate a stale position. A host that stops supplying
audio callbacks while stopped cannot update stopped seeks through this path;
ARA's optional playback controller provides requests, not position notifications.
This must be checked with REAPER's actual processing/suspension settings.

Keyboard focus uses JUCE's `EDITOR_WANTS_KEYBOARD_FOCUS FALSE` plus non-grabbing
editor/control settings. Space requests, when handled, use the optional ARA host
playback controller on the message thread. The host may ignore requests. If the
interface or a transport snapshot is absent, Space is not consumed. REAPER's
per-FX keyboard-routing setting and embedded/floating editors require live checks;
there are no global hooks or synthetic platform keyboard events.

Audition requires the host to assign the ARA editor-renderer role **and** the
selected region/sequence and to call that renderer while stopped. Role absence,
unassigned regions, cache waits and missing stopped callbacks are shown in the
editor. Role presence alone is not proof that audition works in REAPER. There is
no ARA request to wake a suspended editor renderer, and the plugin does not start
host transport to work around that limitation. Preview streams bounded cached
blocks, uses 5 ms preview-only boundary fades, never waits for samples in the audio
callback, and cancels on host playback, item/source edits or editor close. It does
not preview during offline exports.

See [the 0.2.1 REAPER procedure](docs/REAPER-ACCEPTANCE-0.2.1.md) and
[the stabilization record](docs/IMPLEMENTATION-0.2.1.md), and
[the verification report](docs/COMPLETION-0.2.1.md).

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
| `PitchEditor.*` | Piano roll, transport cursor, navigation, Follow, selection and preview controls |
| `TransportState.h`, `EditorCoordinates.h` | Atomic host transport snapshots and testable source/project interval mapping |
| `PitchGardenAudition.h`, `PitchGardenRegion.h` | Optional ARA preview renderer and stable region identity |
| `PitchQuality.*` | Non-destructive pitch uncertainty annotations |
| `PitchGardenARA.h`, `ARAEntry.cpp` | Document controller, processor, editor binding and legacy archive compatibility |

Default analysis: 2048-sample window, 256-sample hop, 65–1200 Hz, YIN threshold
0.15. The window increases to at least `ceil(2 * sampleRate / 65) + 2` samples
when needed (notably at 96 kHz); this prevents truncating the lowest YIN lags.
Frames are timestamped at window centers. Incomplete trailing windows are omitted.
The stereo analysis signal is the arithmetic channel average; opposite-polarity
channels can cancel and are a known limitation.

YIN can choose an octave harmonic even with high confidence: a synthetic 110 Hz
fundamental at amplitude 0.01 plus its 220 Hz harmonic at 0.8 produced about
220.004 Hz with confidence 0.99967. This demonstrates ambiguity, not a diagnosis
of a particular vocal recording. The quality classifier marks excursions near
one/two octaves lasting at most 80 ms only when stable, confident neighboring
pitch returns. A tolerance of 0.65 semitones handles detector jitter. It does not
rewrite pitch or feed corrected data to segmentation. Sustained octave changes,
vibrato and portamento remain raw. Some intentional short octave ornaments may
be marked uncertain; persistent octave mistakes cannot be resolved this way.

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
correct. The old demo loop audition is replaced by the bounded optional editor-renderer
preview described above. Normal host playback still uses the dry renderer.

## Verification

Eight standalone CTest tests cover detector reference frequencies/sample rates,
silence, bad input, conversions/time mapping, note segmentation, cancellation,
publication, shutdown, and unity sample conversion. A ninth optional test creates
an in-process ARA host and exercises real source-reader callbacks, source
analysis, region offsets, dry rendering, moves/trims, stereo 96 kHz replacement,
reader cleanup, nonblocking cache misses with blocked host I/O, and 96-to-48 kHz
rendering. Stabilization tests additionally cover transport snapshots, loop/seek/
stop mapping, Follow decisions, transient octave flags, optional-role behavior,
Space host requests, add-only original-note preview, finish/stop, cancellation,
no offline audition and no host seek/cycle changes. It also writes `pitchgarden-piano-roll.png` in its working
directory for inspection. This is not REAPER compatibility certification.

Follow [the 0.2.1 REAPER acceptance procedure](docs/REAPER-ACCEPTANCE-0.2.1.md) before
calling this stabilization milestone complete. The [0.2 checklist](docs/REAPER-ACCEPTANCE.md)
remains available for baseline playback/analysis checks. The implementation record is in
[IMPLEMENTATION-0.2.md](docs/IMPLEMENTATION-0.2.md).

## Licensing

Project license: GPLv3. The adapted integration retains the upstream JUCE demo's
ISC notice. The pinned JUCE 9 checkout offers commercial/AGPLv3 terms; review
those terms and the ARA SDK licenses before redistribution. No dependency license
compatibility claim is made here.
