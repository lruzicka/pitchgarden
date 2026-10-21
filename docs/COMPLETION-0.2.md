# PitchGarden 0.2 delivery report

The implementation is delivered on `feature/pitch-analysis-0.2`. The full VST3
build and automated tests pass. **The milestone's definition of done is not yet
met: interactive REAPER loading, real vocal recordings, listening checks and
project lifecycle acceptance remain NOT VERIFIED.** Display access is unavailable
in this environment (`wmctrl -l`: Cannot open display), with no GUI automation
or virtual display tools installed. No installed plugin was replaced.

## Architecture and choices

- Existing YIN reused through `pitchgarden_core`; validated continuous MIDI and
  source-time utilities; bounded source-window reads with actual sample rate.
- A source-owned JUCE ARA reader feeds a cancellable offline job. Model callbacks
  stop jobs/readers before invalidation. Immutable snapshots contain only values,
  never ARA pointers. One source result can map to multiple modifications/regions.
- Independent median segmentation retains raw frames and half-open frame ranges.
  Conservative hysteresis, gap and duration thresholds are configurable.
- Custom piano roll clips to current source-offset/region mapping, draws separate
  note labels and continuous original pitch, and provides scrolling, both zoom
  axes, Fit, selection, ranges, status and selected-note details.
- Analysis and segmentation never execute in realtime callbacks or paint.
- Dry playback has no demo dimming. Equal-rate samples preserve unity; differing
  rates use linear sample-rate conversion. A four-window cache reads on a
  time-slice worker and publishes preallocated staging buffers. Atomic lifetime
  gates plus try-locks prevent realtime readers waiting for host I/O. The entire
  host block is preserved if rendering cannot complete.
- The legacy archive layout is accepted, ignoring dim flags. Analysis is in
  memory and is recomputed rather than introducing persistent editing state.
- Pinned dependencies and plugin IDs are unchanged. The pinned JUCE is actually
  9.0.4 plus one commit, so requested JUCE 8 compatibility remains unverified.

## Files

Created:

- `src/ARAAnalysisBridge.{h,cpp}`, `src/PitchAnalysis.{h,cpp}`
- `src/AnalysisModel.{h,cpp}`, `src/AnalyzedSource.h`
- `src/NoteSegmenter.{h,cpp}`, `src/PitchEditor.{h,cpp}`
- `src/PitchGardenARA.h`, `src/PitchGardenPlayback.h`, `src/PlaybackCache.h`,
  `src/DryPlayback.h`, `src/RenderGate.h`
- `tests/analysis_test.cpp`, `tests/model_test.cpp`, `tests/segment_test.cpp`,
  `tests/playback_test.cpp`, `tests/render_gate_test.cpp`,
  `tests/ara_integration_test.cpp`
- `.gitignore`, `docs/IMPLEMENTATION-0.2.md`, `docs/REAPER-ACCEPTANCE.md`, this report

Modified:

- `src/ARAEntry.cpp`, `src/PitchDetector.cpp`
- `CMakeLists.txt`, `README.md`, `scripts/bootstrap.sh`, `scripts/build.sh`

## Actual verification

Environment: Fedora 45, GCC 16.2.1, CMake 4.3.0, C++17, Release builds.

| Check | Actual outcome |
| --- | --- |
| Standalone build, `/tmp/pitchgarden-core` | PASSED |
| Standalone CTest | 6/6 PASSED |
| Full build, `/tmp/pitchgarden-plugin` | PASSED, including VST3 and integration executable |
| Full CTest | 7/7 PASSED |
| In-process ARA source and playback regression | PASSED |
| Display-free integration execution | PASSED without DISPLAY/WAYLAND_DISPLAY |
| Editor image | Generated and visually inspected |
| Bootstrap on initialized pinned dependencies | PASSED, no download |
| Shell syntax and `git diff --check` | PASSED |
| Sanitizer configuration | UNAVAILABLE: compiler linker cannot find libasan.so.8.0.0 / libubsan.so.1.0.0; no sanitizer tests ran |
| Interactive REAPER acceptance | NOT VERIFIED |

Coverage includes silence, reference sines/sample rates, malformed settings,
source timestamps and offsets, vibrato/slides/gaps/confidence, cancellation,
immutable publication, worker shutdown and model-writer exclusion. The ARA test
uses actual factory/model/source-reader callbacks with a deterministic host,
checks reader thread ownership, exact native-rate unity rendering, moves/trims,
separate-document isolation, stereo 96 kHz source replacement, 96-to-48 kHz
rendering, non-ARA/null-playhead passthrough, and reader cleanup.

Independent review identified two issues in the first playback implementation:
cache misses erased input, and JUCE BufferingAudioReader took a blocking mutex.
Both were corrected. A regression with blocked host I/O and a distant source seek
requires realtime rendering to return within 50 ms while preserving every host
input sample. This regression was also run against the previous renderer and
failed on the erased-input assertion, then passed with the fix. The reviewer
rechecked the replacement cache/gates and found no further important issue.

The built bundle is:

`/tmp/pitchgarden-plugin/PitchGardenARA_artefacts/Release/VST3/PitchGarden.vst3`

## Limitations and remaining acceptance

- No interactive REAPER evidence; all rows in `REAPER-ACCEPTANCE.md` remain
  NOT VERIFIED, including vocal listening, splits, undo/redo, save/reopen,
  multiple live plugin instances, shutdown and rapid seeks.
- Unsupported time/rate stretching withholds curves and retains host input;
  fallback audio depends on the host's input behavior.
- Linear sample-rate conversion is not band-limited or transparent relative to
  REAPER's resampler. Cache misses preserve host input; that may not contain the
  audible dry source in every ARA host. More than four distant regions of a source
  can exceed cache capacity. These must be assessed in REAPER.
- Stereo averaging can cancel opposite-polarity content. Large vibrato or slow
  slides can require segmentation threshold tuning.
- Per-source workers can use significant CPU with many long recordings.
  Cancellation waits on the model thread for the current host read to finish.
- The demo's loop audition is omitted because its sample-rate mapping was not
  robust. Normal host playback uses the dry renderer.
- JUCE 8 and sanitizer operation have not been validated here.

## Commits

- `525e252`: Phase A — incremental ARA analysis bridge and validated coordinates
- `9ebb87a`: Phase B — cancellable source-owned analysis lifecycle
- `9abd04f`: Phase C — conservative note segmentation
- `1394e11`: Phase D — piano roll and mapped dry playback
- Phase E: the commit containing this report — review fixes, expanded integration
  regressions, build/bootstrap repair and acceptance documentation

No commits were pushed or merged into master.
