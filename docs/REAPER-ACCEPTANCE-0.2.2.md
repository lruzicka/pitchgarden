# Fedora / REAPER verification for 0.2.2

All live REAPER rows are **UNVERIFIED**. Record actual results and versions;
automated JUCE/ARA tests are not a substitute. Use a disposable project and keep
the original vocal capture for comparing diagnostic readings. Do not overwrite
an active project's plugin bundle.

1. Run `scripts/build.sh`. Quit REAPER, back up the installed plugin, and install
   `build/PitchGardenARA_artefacts/Release/VST3/PitchGarden.vst3` in your VST3
   directory. Restart/rescan. Record Fedora, REAPER, X11/Wayland/XWayland, audio
   device, plugin commit, source/host rates and floating versus embedded editor.
2. Insert the plugin with ARA on a mono vocal item and on an opposite-polarity
   stereo test item. Wait for analysis. Keep ordinary playback audio unmodified.
3. For the original misordered notes, enable Diagnostics and hover at each note's
   center and transition. Capture source time, raw Hz/MIDI/confidence, accepted
   pitch/correction flag, and representative note pitch. Compare to the waveform
   spectrum in an independent analyzer and listen to the original recording.
   If a note remains wrong, retain the audio excerpt and diagnostic values.
4. Compare a known descending phrase (246.94 then 196 Hz with a dominant second
   harmonic on the lower note), known sine octaves, vibrato and glissando. Higher
   MIDI must appear above lower MIDI. Verify true octave changes remain intact.
5. With main transport stopped, press and hold a note. Listen for original audio
   starting at the note's source beginning (clipped to the visible item). Release
   inside, outside the plugin and outside its native window. Test a quick click,
   long hold past the note end, repeated press and dragging away while held.
6. Repeat while closing/hiding the editor, switching applications, changing host
   selection, replacing source audio, trimming/moving/removing an item, starting
   main playback and closing the project. Preview must stop, never restart from
   an old request, never overlap, and never alter the source item or note row.
7. If no preview is audible, record the status: absent role, unprepared renderer,
   unassigned region, no stopped callbacks, or buffering. Keep stopped FX
   processing enabled where supported. ARA editor-renderer assignment and
   realtime callbacks are required; starting REAPER transport is not a workaround.

## Keyboard matrix

Test floating FX and the embedded chain separately, first with REAPER's per-FX
**Send all keyboard input to plug-in** disabled. Inspect the FX chain Options
keyboard-routing choice too; record exact wording/settings for the tested version.
Then repeat with each setting changed one at a time. Do not use global shortcut
bindings to mask the focus failure.

| Action | Expected | Live result |
| --- | --- | --- |
| Click a note, release, press/release Space twice | REAPER starts then stops once each | UNVERIFIED |
| Click Fit, Follow, Diagnostics, blank piano roll, then Space | Same transport behavior | UNVERIFIED |
| Hold Space | At most one transport request until release | UNVERIFIED |
| Hold Space, switch focus away, release elsewhere, return, Space | Next press works; no stuck repeat latch | UNVERIFIED |
| Normal REAPER shortcut and modified Space | Plugin does not consume them; record whether host receives them | UNVERIFIED |
| Seek, play, stop and loop | Cursor/follow retain existing behavior | UNVERIFIED |
| Close/reopen editor repeatedly | No stale listener, crash or preview | UNVERIFIED |
| Main playback and offline render | Original samples at unity; no preview in export | UNVERIFIED |

Plain Space has a supported ARA-controller fallback when delivered to JUCE's
native content wrapper. The host can omit that optional controller or ignore its
requests. Returning false for other keys cannot repair JUCE X11 focus ownership.
If host shortcuts still disappear, click REAPER's arrange/transport area before
using them. This is the smallest manual focus workaround; verify it in the test
session and record the result. Do not apply the closed JUCE PR #1715 patch.

Compare normal playback against bypass (including offsets, stereo, splits,
multiple sources and instances). Equal-rate cached rendering should remain unity.
Existing linear rate conversion is not expected to null against a band-limited
host resampler. Check cache-miss playback and project shutdown while analyzing.

Mark acceptance complete only after actual keyboard, listening, outside-release,
lifecycle and transparency results are recorded, or document the demonstrated
host limitation with its tested workaround and exact configuration.
