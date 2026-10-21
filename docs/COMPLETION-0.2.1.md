# PitchGarden 0.2.1 verification report

Implementation is on `feature/transport-editor-0.2.1`, based on 0.2 commit
`8d99814`. No push or merge was performed. Existing dependency pins, VST3/ARA
identity and original pitch data are preserved; no SDK source was modified.

**Live REAPER acceptance remains NOT VERIFIED.** This session cannot open the
host display (`wmctrl -l`: Cannot open display), and no virtual-display or GUI
automation executable is available. The user's report establishes that existing
0.2 note visualization/navigation work, but does not verify these new features.
The milestone's live transport/keyboard acceptance gate is therefore still open.

## Implemented

- Project-time playback cursor and stopped cursor from audio-thread host
  transport snapshots; seek/loop updates are used directly, without extrapolation.
- Optional Follow, off by default and suspended by scrolling, zoom or Fit.
- JUCE non-grabbing keyboard focus for editor and controls. Plain Space requests
  ARA host play/stop only when the event reaches the editor, the optional host
  controller exists and a transport snapshot is available. Auto-repeat is
  suppressed. Other keys/modified Space remain unhandled.
- Original-note preview through the optional ARA editor-renderer role. Requests
  carry region IDs, source revisions, source interval and region mapping, never
  retained host pointers. Preview is streamed from the existing cache with short
  preview-only fades; no host seek/play/cycle changes or track processing edits.
  It stops at the note boundary and cancels on Stop, editor close, host playback,
  region changes, source invalidation or removal. Offline exports omit preview.
- Explicit audition status for missing role, unprepared renderer, unassigned
  region, queued/missing host callbacks, cache waits and cancellation.
- Conservative uncertainty annotations for short octave-like out-and-back
  excursions and low confidence. Original frequency/MIDI/confidence and note
  segmentation remain unchanged. Vibrato/slides and sustained octave changes are
  preserved. Curves do not connect uncertain or discontinuous octave jumps.
- Correct source-offset/region/project mapping, clipped audition intervals, and
  playback fallback for hosts supplying project seconds without sample position.

## Investigation and important limits

A deterministic weak 110 Hz fundamental plus strong 220 Hz harmonic causes the
existing YIN to report about 220.004 Hz at confidence 0.99967. This demonstrates
harmonic ambiguity even with high confidence. It is not evidence that any
particular user's vocal is an octave error. Annotations require confident stable
neighbors and a return within 80 ms; intentional short ornaments may be flagged,
and persistent octave errors cannot be inferred or corrected this way.

ARA supplies an optional editor-renderer role, but role presence is not proof
that REAPER assigns the selected region or calls it while stopped. Missing
callbacks are reported after a queued request. There is no ARA request to wake
that renderer; audition does not start track playback as a workaround. Actual
REAPER audition capability is unverified, not declared unsupported or supported.

Host play/stop requests may be ignored. Native Space routing, floating/embedded
focus and REAPER keyboard settings require live testing. Stopped seeks cannot
update if the host supplies no processor callbacks; ARA playback control offers
requests but no position notifications. The cursor honestly reports only the
last supplied position. Existing linear-resampler/cache limitations still apply.

## Verification performed

| Check | Result |
| --- | --- |
| Standalone DSP build | PASSED |
| Standalone CTest | 8/8 PASSED |
| Full Linux VST3 build | PASSED |
| Full CTest including in-process ARA host | 9/9 PASSED |
| Display-free ARA integration executable | PASSED |
| Editor snapshot with cursor/Follow/capability status | Generated and visually inspected |
| Independent code review | Important cancellation/lifecycle findings fixed; follow-up found no remaining important gate issue |
| Dependency/source-pin diff | Unchanged |
| Whitespace checks | PASSED |
| Actual REAPER Space, cursor, seeking/looping and audition | NOT VERIFIED |

New tests exercise project seconds/sample conversion, overflow rejection,
nonzero/negative timeline positions, source offsets and trims, seek/loop/stop
snapshots, coherent publication, Follow decisions and actual manual-wheel
suspension, no-focus controls, optional ARA Space requests, additive original-note
preview, completion/stop, offline omission, host-play cancellation, mapping/source
invalidation and a coordinated stop/render race. Existing unity rendering,
source acquisition, stereo/rate replacement, analysis and segmentation tests pass.
The seconds-only playback test and cancellation test were observed failing before
their fixes. Octave classification was tested against a failing implementation
stub, then stable notes, transient up/down octave excursions, sustained octaves,
vibrato, portamento, silence, low confidence and boundary cases.

Realtime publication and preview use bounded atomics/try-read gates and
preallocated scratch/cache storage. Non-realtime request/configuration writers
are serialized separately; the audio callback never acquires that mutex, waits
for analysis or performs source acquisition/YIN. Stop drains any in-flight preview
block before returning, preventing stale playing status after cancellation.

## Files

Created `src/TransportState.h`, `src/EditorCoordinates.h`,
`src/PitchGardenRegion.h`, `src/PitchGardenAudition.h`, `src/PitchQuality.{h,cpp}`,
`tests/transport_test.cpp`, `tests/quality_test.cpp`, and the 0.2.1 implementation,
acceptance and completion documents. Modified CMake/version, README, analysis
model, analyzed-source revision tracking, piano roll, processor/controller,
playback timing and the existing ARA integration test.

The local bundle is
`/tmp/pitchgarden-plugin/PitchGardenARA_artefacts/Release/VST3/PitchGarden.vst3`.
Its generated module metadata reports version `0.2.1`. No installed plugin bundle
was replaced.

## Commits and manual gate

- `0399c87`: transport cursor, Follow and focus/Space handling
- `283d5ed`: optional original-note ARA audition
- `ab3191f`: non-destructive octave uncertainty
- `9c110d3`: serialized audition cancellation/lifecycle and concurrent regression
- `90d4e27`: seconds-only host timing fallback
- Release/documentation: the commit containing this report

Run [REAPER-ACCEPTANCE-0.2.1.md](REAPER-ACCEPTANCE-0.2.1.md), recording actual
Space routing, cursor behavior on play/seek/loop/stop, Follow suspension, note
preview capability (or the observed host limitation), item moves/trims/splits,
multiple instances and unchanged track audio. Do not mark the milestone complete
until those live transport/keyboard checks pass and audition capability or its
observed limitation is documented.
