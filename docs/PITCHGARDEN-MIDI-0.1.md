# PitchGarden MIDI 0.1

PitchGarden MIDI is an independent monophonic audio-to-MIDI ARA VST3 plugin.
It does not correct audio, quantize, synthesize an instrument, recognize chords,
or implement a MIDI note editor. The original PitchGarden remains available.

**Automated implementation/build checks pass. Interactive native REAPER loading,
external drag, native insertion, listening and coexistence checks are UNVERIFIED.**
The release's live acceptance gate remains open until the procedure below is run.

## Build and install on Fedora

Use the existing dependencies described in the repository README (C++17, CMake,
ALSA, FreeType/Fontconfig and X11 development libraries). JUCE is still pinned to
`454fe891acc8204f99f4ef1e3a28c84b1a4da145` (headers 9.0.4); ARA remains
`a2b1aac1d1d5c4eed387db85a9c0cdb7d460254c` with its existing API/Library pins.
No SDK sources or gitlinks changed. Normal configure/build does not fetch data.

```bash
./scripts/bootstrap.sh  # Only needed if pinned dependencies are absent.
./scripts/build.sh
python3 scripts/install.py
```

This builds and installs both `PitchGarden.vst3` and `PitchGarden MIDI.vst3` into
`~/.vst3`. Previous bundles are retained outside that scan directory in
`~/.pitchgarden-plugin-backups/<unique-install-id>/`. Copying is staged and
renamed; a failed install rolls back the completed replacements. The installer
does not close REAPER or change a running project's FX. Restart REAPER and rescan
the VST3 path to load the installed builds. Both should appear independently;
actual FX-browser visibility requires the live test.

Build products:

- `build/PitchGardenARA_artefacts/Release/VST3/PitchGarden.vst3`
- `build/PitchGardenMIDI_artefacts/Release/VST3/PitchGarden MIDI.vst3`

For core-only tests without JUCE or ARA:

```bash
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --parallel 4
ctest --test-dir build-core --output-on-failure
```

## Workflow

1. Record monophonic audio, then add PitchGarden MIDI to the item through
   REAPER's ARA workflow. Select exactly one playback region/item.
2. Wait for analysis. The read-only roll shows notes audible in that region,
   with region-relative seconds and MIDI pitches. The count reflects exportable
   notes. Pauses are preserved. Analysis supports fundamentals from 30 to 2000 Hz;
   original PitchGarden defaults are unchanged.
3. In a verified constant-tempo project without tempo/time-signature markers,
   drag **Drag MIDI to REAPER** onto the desired destination track at the original
   audio item's project position. The file uses that project's explicit tempo.
   Host import behavior still needs live verification; do not import its tempo
   map into an unrelated project assuming it will align automatically.
4. For tempo maps or when drag fails, select exactly one destination track in
   REAPER, keep the original source item available, and click **Insert on selected
   track**. This creates a new MIDI item at the source region's project start and
   duration. Each note boundary is mapped through REAPER's native project timing.
   It does not move the edit cursor, start playback or change the original item.
5. **Export MIDI** independently saves a normal `.mid` file. Choose a name ending
   in `.mid`; the exact confirmed filename is written. In an unknown or changing
   project tempo, this portable file uses 120 BPM. Its elapsed-time meaning is
   self-contained, but arbitrary host import into a different tempo can stretch
   it. Use native insertion for project alignment in that case.
6. Edit the new item in REAPER's MIDI editor. Delete the original audio only after
   verifying the transcription. No automatic deletion is performed.

No source, unavailable samples, silence/unreliable detections, multiple regions
and stretched/rate-transformed regions produce explanatory status and disabled
exports as appropriate. Multiple regions are never silently concatenated.

## Timing and conversion contract

The converter is separate from DSP and independently testable. It uses existing
representative MIDI pitches; values outside 0–127, nonfinite pitch/time and
invalid/zero durations are rejected rather than clamped into arbitrary notes.
Every note uses velocity 100 and MIDI channel 1 (channel index 0 in REAPER's C API).

For each source note `[a,b)` and region source interval `[s,s+d)`, export
`[max(a,s)-s, min(b,s+d)-s)`, discarding empty intersections. The MIDI item is
placed at the region's project start. Source offsets are not added twice.
Stretched mappings or unequal source/playback durations are rejected.

Same-pitch overlaps cut the preceding note at the next onset. Identical onsets
coalesce to the longest duration. Touching notes remain separate. Different
pitches may overlap deterministically. Note Off events sort before Note On at
equal ticks, then by pitch, preventing an old Note Off from ending a repeated
note. Normalization is repeated after tick rounding to handle collapsed onsets.

SMF format 0, one track, 960 PPQ, and an explicit tempo event at tick zero are
written with JUCE. Seconds convert to nearest integer ticks using the encoded
microseconds-per-quarter-note value; no musical quantization is applied. Tempo
metadata has the MIDI-standard integer-microsecond precision. File reimport
preserves elapsed note timing within half a tick. Detected attack boundaries retain the analysis window/hop resolution; MIDI conversion does not invent sample-accurate onsets. The End Of Track preserves the
region duration including leading/trailing silence. Notes collapsing to zero
ticks are omitted; a completely empty event set is never saved as a transcription.
The implementation rejects durations over 0x0fffffff ticks rather than overflowing
SMF variable-length fields. File replacement is staged through JUCE TemporaryFile.

For direct drag, a conservative REAPER gate requires a valid instance project,
`CountTempoTimeSigMarkers == 0`, and a valid `GetProjectTimeSignature2` BPM.
Even a purely time-signature marker disables this route. This prevents falsely
using that API (which ignores tempo envelopes) as a complete tempo-map model.
Portable files use the quantized explicit tempo; long files at tempos not exactly
representable in microseconds can have small additional drift when a host ignores
file tempo and uses its own exact BPM. Native insertion avoids that ambiguity.

The compiled REAPER bridge uses the published
[IReaperHostApplication](https://github.com/justinfrankel/reaper-sdk/blob/main/sdk/reaper_vst3_interfaces.h).
It resolves and validates the plugin instance's project for every operation,
requires one selected destination track, creates a new item in project seconds,
and calls `MIDI_GetPPQPosFromProjTime` for **each** boundary before
`MIDI_InsertNote`. This delegates tempo changes/ramps to REAPER rather than
approximating a map in the plugin. A failed insertion removes only the newly
created item and closes its undo block. A cleanup failure is reported explicitly.
These APIs are documented in the [REAPER SDK](https://www.reaper.fm/sdk/reascript/reascripthelp.html).
Tests verify a nonlinear host timing double, not actual REAPER tempo behavior.

## Linux drag behavior and temporary-file lifetime

The pinned JUCE implementation uses X11 XDND file URI lists. VST3's Linux editor
integration in this checkout uses `X11EmbedWindowID`. X11 and XWayland are the
relevant paths; native Wayland file-drag compatibility is not claimed. Embedded
and floating native REAPER editors must each be checked. A successful standalone
or in-process GUI test is not evidence that an embedded VST3 drag works.

A unique UUID `.mid` file is fully written before drag starts. Only a genuine
left drag crossing the control's threshold calls
`DragAndDropContainer::performExternalDragDropOfFiles(files,false,&source,callback)`.
Copy semantics are requested. Drag completion does not certify that REAPER imported
an item. Selection or tempo changes invalidate the prepared snapshot before use.
The callback uses a safe component reference; closing the editor is safe.

Files are deliberately retained beyond callback, editor destruction and plugin
unload in `$TMPDIR/PitchGarden-MIDI-<login>/` (normally `/tmp/...`). This avoids a
race with delayed host import and keeps a host-referenced file available. The
plugin never automatically deletes that directory; the user/OS may clean it after
REAPER has imported/copied the data. Two instances use different filenames. A
missing file or failed write disables drag with a visible reason, without a
250 ms disk-write retry loop. Normal file export remains independent. Reselect
the source or change relevant tempo/source state to prepare another drag.

If XDND fails, use Export MIDI or the native Insert button. The REAPER-specific
alternative is already compiled into the plugin, so no extra extension, OSC
server, network, ReaScript installation or global input simulation is needed.

## Implementation and regression scope

- `pitchgarden_core`: unchanged algorithms; source configuration is optional.
  Original default analysis remains 65–1200 Hz. MIDI alone selects 30–2000 Hz;
  reader buffer sizing and worker windows use the same configuration.
- `pitchgarden_midi_conversion`: validated region clipping and deterministic
  event/tick conversion, with no JUCE dependency of its own.
- `MidiFileExport`: JUCE SMF encoding, atomic export and retained drag files.
- `MidiEditor`: single-region immutable snapshots and compact read-only view.
- `MidiReaperHost`: UI-thread-only host API lookup, context and MIDI insertion.
- `MidiPlugin`/`MidiEntry`: shared processor/controller behavior with independent
  version 0.1.0, code `PgM1`, bundle `org.pitchgarden.midi`, factory
  `org.pitchgarden.midi.arafactory.0.1.0` and archive
  `org.pitchgarden.midi.aradocumentarchive.1`.

Original plugin code `PgA1`, IDs, editor, audition renderer, transport keyboard
handler, cursor navigation and normal playback implementation remain unchanged.
The shared controller was made derivable; source/model methods accept optional
settings while preserving old calls and defaults. No recording I/O or analysis
was added to the real-time callback. Existing cache/resampling limitations remain.
The analysis is still monophonic and may make recognition errors on noisy,
polyphonic or ambiguous recordings.

## Verification performed

| Check | Result |
| --- | --- |
| Both Release VST3 targets | PASSED |
| Full CTest | 16/16 PASSED |
| Clean core-only configure/build/CTest | 10/10 PASSED |
| MIDI conversion and SMF reimport | PASSED |
| Real ARA reader/worker at 44.1/48 kHz, five bass notes, pauses | PASSED |
| Nonzero project positions, source offsets, trims and multi-region rejection | PASSED in-process |
| Native seconds-to-PPQ tempo changes, rollback, API/project/thread validation | PASSED against host double |
| Simultaneous loading of both Linux DSOs and distinct live ARA factories | PASSED |
| Unique retained drag files and one-start-per-gesture behavior | PASSED |
| Original audition, cursor, transport and dry-playback automated regressions | PASSED |
| Actual REAPER FX browser/loading, drag, insertion, MIDI edit and listening | UNVERIFIED |
| Native Wayland / embedded and floating REAPER drag | UNVERIFIED |

Review findings for overwrite-path confirmation and drag-write failure retries
were fixed. The low-bass ARA test was observed failing with the original range,
then passing with MIDI-specific configuration. The definition of done's live
host requirements are not declared satisfied by these automated results.
