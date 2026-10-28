# Native Fedora / REAPER acceptance: PitchGarden MIDI 0.1

All interactive rows below are **UNVERIFIED** by the implementation session.
Use a disposable project. Record Fedora, REAPER, X11/Wayland/XWayland, audio rate,
plugin commit, and embedded/floating editor mode. Build/install with
`scripts/build.sh` and `python3 scripts/install.py`, restart REAPER, then rescan.
Do not replace an active source item or delete audio until the MIDI is checked.

Reproducible optional fixtures:

```bash
python3 scripts/generate_midi_test_audio.py
python3 scripts/generate_midi_test_audio.py --prefix-seconds 3 --output build/midi-fixtures-offset
```

The first directory contains 44.1/48 kHz guitar and bass WAVs, each with five
0.3-second tones and pauses. MIDI pitches are printed by the generator. The second
adds three seconds of leading silence for the source-offset test. The automated
ARA test also writes `build/pitchgarden-midi-five-notes.mid` from its detected bass
notes, which can be imported independently. Analysis localizes onsets with a
window/hop; export preserves those detected boundaries rather than promising
sample-accurate transcription of attacks.

1. Confirm **PitchGarden** and **PitchGarden MIDI** appear as distinct ARA VST3 FX.
   Add MIDI to a mono recording of five separated notes. Wait for automatic
   analysis, confirm five note rectangles/count, and export a `.mid` file.
2. On a second track with an instrument of your choice, in a constant-tempo
   project **without tempo/time-signature markers**, drag **Drag MIDI to REAPER**
   onto the corresponding original project start. Confirm a MIDI item is created
   and opens in REAPER's native MIDI editor. Decline any unnecessary tempo-map
   replacement prompt. Compare note pitches, starts, lengths and pauses.
3. Repeat at 90 BPM and 120 BPM, with audio at project 12 s. Verify the first note
   is relative to 12 s rather than source zero. Repeat after moving the item to
   30 s. Use a trimmed item with source offset 3 s; notes before that source
   offset must be absent, and boundary notes must be clipped.
4. Add a tempo change inside a sustained note, then a ramp. File drag should be
   disabled with an explanation. Select exactly one destination track and press
   **Insert on selected track**. Verify the new item's original project position
   and that note boundaries match the audio in **seconds** on both sides of the
   tempo change. Check the sustained note crossing the change. The project's
   tempo map and original item must remain unchanged. Test undo once.
5. Export normally in the variable-tempo project. The portable file explicitly
   uses 120 BPM: opening it with its own tempo should preserve elapsed timing.
   Importing it while ignoring its tempo is not the alignment-safe route; use
   native insertion. Do not mark variable-tempo support passed solely from drag.
6. Repeat with silence, pauses and separate 44.1/48 kHz recordings. Silence/no
   stable notes must not enable misleading output. Select multiple source items
   and a stretched/rate-transformed item: export should explain the limitation.
7. Use two MIDI instances in one project. Drag/export each, cancel a drag, close
   an editor during/after drag and change source selection. Each file must remain
   readable and correspond to its own region; no stale-source export or crash.
8. Test both floating and embedded FX under X11, then XWayland if available.
   Record whether XDND starts and creates a MIDI item. If unavailable, test both
   independent file export and native Insert, and record the exact failure/status.
9. With the original PitchGarden in the same project, independently verify
   analysis, note selection, hold/release audition including release outside,
   Space start/stop, blank-background edit-cursor clicks, ruler drags, wheel zoom/
   scroll, and unchanged audio playback. Close/reopen the project and both FX.

| Test | Actual result |
| --- | --- |
| Separate FX identities load and coexist in REAPER | UNVERIFIED |
| Five notes and pauses exported/imported correctly | UNVERIFIED |
| Nonzero project start, source offset and clipping | UNVERIFIED |
| 44.1 kHz / 48 kHz recording | UNVERIFIED |
| Constant-tempo drag creates editable MIDI item | UNVERIFIED |
| Tempo-change/ramp native insertion preserves seconds | UNVERIFIED |
| Two instances, cancellation and retained files | UNVERIFIED |
| Embedded/floating X11/XWayland drag | UNVERIFIED |
| Export MIDI works independently of drag | UNVERIFIED |
| Original PitchGarden audition/Space/cursor/audio | UNVERIFIED |

If drag fails, retain the export file, record desktop/backend/editor mode and
status, and use the normal file export/native insertion routes. If either timing
or original-plugin behavior regresses, acceptance remains open even when a MIDI
file was successfully produced. Do not call the 0.1 live integration complete
until real results replace these entries.
