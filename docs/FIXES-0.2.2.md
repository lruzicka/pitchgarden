# PitchGarden 0.2.2 investigation and verification

Interactive REAPER acceptance is **UNVERIFIED**. Automated core, ARA-host and
native JUCE peer tests are distinguished below; none certifies real REAPER key
routing, stopped preview scheduling or audible playback. The user's original
vocal recording was not available, so its particular mismatches remain unverified.
No SDK/submodule, original audio item, pitch correction DSP or note drag was added.

## Pitch path and root causes

1. ARA acquisition uses `ARAAudioSourceReader` with integer source sample offsets,
   the source's declared sample rate and separate float channel buffers. Reads
   remain on the analysis worker, with reader destruction after cancellation.
   Existing 48/96 kHz ARA tests validate indexing, offsets and source replacement.
   **Proven channel bug:** arithmetic stereo averaging cancels opposite-polarity
   audio and can suppress a fundamental while retaining even harmonics. Analysis
   now takes the higher-energy channel per window (left on ties). This policy is
   for monophonic recordings; it is not a polyphonic stereo pitch estimator.
   Playback channels remain unchanged. The opposite-polarity 96 kHz source
   previously produced no notes; the same host test now detects MIDI 57 (220 Hz)
   and renders the original, opposing stereo channels correctly. A separate
   44.1 kHz stereo ARA source cancels its 196 Hz fundamental in the channel
   average while retaining 392 Hz; independent-channel analysis detects MIDI 55.
2. The YIN difference function compares a fixed `n-hi` sample interval for every
   lag. CMND uses `d[tau]*tau / sum(d[1..tau])`. No normalization/indexing error
   was found. **Proven octave bug:** first threshold crossing accepts a half
   period when the second harmonic dominates, even at high confidence.
   Confidence `1-CMND` measures periodicity, not certainty about the octave.
3. Both estimator and analysis use `69 + 12*log2(hz/440)`; known-frequency tests
   confirm conversion. Frame time is the source window center, without project
   offsets or host-rate substitution. Windows grow to cover low lags at high rates.
4. Segmentation takes the median of accepted continuous MIDI measurements and
   rounds only the displayed note row. It propagated the estimator's sustained
   octave mistake; it did not originate the reproduced error. Persistence/gap
   settings and representative-pitch estimation remain unchanged.
5. `PitchEditor::y` decreases as MIDI increases, for both curve and note rows.
   Higher pitches correctly draw above lower pitches. Hit tests on the expected
   MIDI 57 row pass through the actual editor mapping. No inverted coordinate
   conversion was found or patched.

### Conservative period selection

After retaining the original first-minimum observation, search near twice its
lag, within the existing supported range. Accept a candidate only when its CMND
improves by more than 0.02 and falls below 20% of the initial value, its pitch is
within 0.5 semitones of one octave lower, and a separate Hann-windowed spectral
projection detects the candidate's fundamental or third harmonic above 0.12 of
signal RMS. Equal-quality multiple periods of a pure sine do not qualify.

The offline source pipeline additionally requires at least two nearby, voiced
measurements (within two hops, confidence >= 0.7) agreeing within 0.7 semitones.
The immutable candidate series makes this check independent of traversal order.
Continuity alone never changes a pitch. Raw Hz, continuous MIDI and confidence
are retained on every voiced observation, alongside the accepted measurement
and `octaveCorrected` flag. This changes detected pitch estimates, never audio.

An almost absent fundamental (e.g. amplitude 0.01 vs second harmonic 0.8) may
remain unresolved deliberately. There is no safe general guarantee for noisy,
polyphonic or mathematically ambiguous recordings, nor a forced neighboring octave.

Enable **Diagnostics** and hover in the plot to see source time, raw YIN Hz,
raw continuous MIDI, raw confidence, accepted MIDI/confidence, correction flag
and the representative note MIDI. Disable it to restore the unobscured plot.
The white curve and note medians use accepted measurements; raw values remain
available for investigating new recordings.

### Reproducible before/after evidence

`tests/accuracy_test.cpp` constructs `0.12*sin(p) + 0.65*sin(2p) + 0.035*sin(3p)`
at 48 kHz. An independent Hann-windowed DFT diagnostic confirms amplitudes
0.119992 and 0.649955 at the fundamental and second harmonic. It does not use
YIN. The test was observed failing against the original core before the fix.

| True frequency | Original YIN, Hz | Accepted after fix, Hz |
| --- | --- | --- |
| 110 | 220.393 | 110.000 |
| 196 | 393.619 | 196.002 |
| 220 | 440.796 | 220.002 |

Original readings remain in `rawHz/rawMidi/rawConfidence` after correction.
The same executable asserts known sines at 44.1, 48, 96 and 192 kHz; weak/missing
fundamentals; a 246.94 to 196 Hz descending phrase (MIDI 59 to 55); genuine
sustained and brief octave changes; phase-continuous 5 Hz vibrato; a seven-semitone
continuous glissando; and seeded low-confidence noise. No original vocal capture
was supplied, so this proves reproducible mechanisms, not the cause of every
reported sung note.

## Keyboard investigation and plugin fix

The unchanged JUCE gitlink is `454fe891acc8204f99f4ef1e3a28c84b1a4da145`,
headers 9.0.4. `EDITOR_WANTS_KEYBOARD_FOCUS FALSE` and non-grabbing controls remain.
In this pin, X11 button handling calls `peer->toFront(true)` and native
`LinuxComponentPeer::toFront` calls `grabFocus`. Returning false from an editor
key handler cannot undo native focus ownership. This agrees with
[JUCE issue #1171](https://github.com/juce-framework/JUCE/issues/1171).
[PR #1715](https://github.com/juce-framework/JUCE/pull/1715) was closed after
inconsistent results and popup/text-focus regressions; its patch was not applied.

A second, reproducible plugin-level issue prevented the existing ARA Space
fallback: with no logically focused child, `ComponentPeer::getTargetForKeyPress`
selects the top-level VST3 content wrapper, not the nested processor editor.
The editor's handler was bypassed. The fix installs a JUCE `KeyListener` on that
wrapper while attached, using a `SafePointer` and removing the listener on
reparenting/destruction. An editor-owned 30 Hz watchdog clears the repeat latch
when its own native peer loses focus or Space is no longer down, covering a
release delivered elsewhere. It stops before listener teardown. Plain Space reaches the existing optional ARA playback
controller on the message thread; autorepeats are suppressed. Modified Space and
all other keys are unhandled. No native/global hook or key synthesis is used.

An opt-in native test opens only its own temporary JUCE window and calls its
peer's key dispatch directly, without sending any OS key events:

```bash
build/pitchgarden_ara_test --native-keys
```

It failed at `peer->handleKeyPress(Space)` before this fix; it passes after the
fix, including an ARA stop request, an unhandled `x`, and stale-latch recovery after
a lost release. The last test dispatches keys inside JUCE without changing
physical key state; it verifies watchdog clearing, not real OS focus changes. Default integration
checks also verify start/stop, repeat suppression and no host seek/cycle changes.
This is native JUCE dispatch evidence, **not interactive REAPER verification**.

The inspected VST3 `IPlugView::onKeyDown/onKeyUp` defaults return unhandled; the
wrapper does not implement general native-key forwarding to the host. JUCE's
VST3 client-extension callbacks are not a general transport or keyboard forwarding
API. ARA's optional playback controller is the supported available transport path;
the host can omit it or ignore requests. Other native REAPER shortcuts can still
be affected by #1171. Do not report all shortcuts restored from a false return.

Practical fallback: click REAPER's arrange/transport area before pressing host
shortcuts. This returns input to a host window and requires no plugin changes.
Check REAPER's per-FX **Send all keyboard input to plug-in** setting and the FX
chain Options keyboard-routing setting (wording varies by version), first with
forced sending disabled. Neither those settings nor this workaround were
interactively verified here. Do not apply global action bindings or global hooks.
See the explicit manual procedure below.

## Hold-to-audition

Left press selects a note and immediately submits its original, clipped source
interval to the existing single ARA editor renderer. Playback begins at that
interval's start, never at the clicked timeline point or from synthesized MIDI.
There is no looping and no concurrent preview voice. Selection/pitch/time remain
unchanged. Release stops it; JUCE drag capture delivers releases outside the
component. A 30 Hz native button-state/deactivation/visibility watchdog also
cancels a lost release. Disabling/hiding/closing the editor, changing displayed
sources/selection, host playback, source revisions and mapping changes cancel.
The Audition and Stop preview buttons have been removed.

The optional ARA editor-renderer role and assigned region/sequence are required.
The SDK specifies permanent realtime rendering for editor renderers; exports do
not audition. No transport start/seek is requested to wake a suspended renderer.
The status reports missing roles, assignments, preparation and stopped callbacks.
Actual REAPER stopped rendering and audible hold/release remain **UNVERIFIED**.
A host that does not supply this role/context cannot audition by this route.

The audio callback continues to use preallocated scratch space and cache-only
nonblocking reads; no I/O, analysis or GUI mutex was added. Stop drains an
in-flight preview on the non-audio caller. Assignment removal now invalidates
immediately, preventing a removed/reassigned region from reviving a stale request
before the next callback. This failure was reproduced by the integration test.

Integration assertions cover source audio samples, clipping, fades, natural end,
explicit stop, outside-position release, repeated press, disable, closure, source
replacement, mapping invalidation, assignment remove/readd, host-play cancellation,
non-realtime omission, concurrent stop/render and unchanged host transport/input.

## Automated verification

- Full Release VST3 build: passed.
- Full CTest: 10/10 passed, including in-process ARA host.
- Clean core-only Release build and CTest: 9/9 passed.
- Native JUCE peer test: passed (plain Space, unhandled key and stale-latch recovery).
- No SDK source or gitlink changes; no installed plugin was replaced or pushed.

Bundle: `build/PitchGardenARA_artefacts/Release/VST3/PitchGarden.vst3`.
The manual procedure is [REAPER-ACCEPTANCE-0.2.2.md](REAPER-ACCEPTANCE-0.2.2.md).
