# Fedora / native REAPER acceptance — PitchGarden 0.2

**All interactive REAPER checks below: NOT VERIFIED.** The in-process ARA host
regression is automated evidence, not a substitute for this procedure. The
milestone is not complete until these checks are actually performed successfully.

## Preparation

1. Build and test using `scripts/bootstrap.sh` then `scripts/build.sh`.
2. Quit REAPER, copy `build/PitchGardenARA_artefacts/Release/VST3/PitchGarden.vst3`
   into `~/.vst3/`, and restart/rescan plugins. Retain a backup of the old installed
   bundle if replacing one. Record Fedora, REAPER, compiler and plugin revisions.
3. In REAPER's ARA preferences, enable ARA support as appropriate for that version.
   Insert PitchGarden on the recorded take/item using REAPER's ARA workflow.
   Verify that the editor reports source analysis rather than waiting indefinitely.
4. Use a disposable project with a real mono vocal, a stereo recording, audio
   containing silence and multiple stable notes, and expressive vocals with vibrato
   and portamento. Include 44.1, 48 and 96 kHz sources. Do not replace the vocal
   listening checks with sine waves. Note the recording's approximate pitch range.
5. Save a dry reference render before adding the plugin. Disable unrelated effects,
   normalization, automatic fades and dither when doing a numerical null test.

## Test matrix

Record actual outcomes, audio glitches and crash logs in a copy of this table.

| Check | Procedure and expected behavior | Status |
| --- | --- | --- |
| Load and source access | Add ARA PitchGarden to mono vocal; progress advances and notes/curves appear | NOT VERIFIED |
| Stereo | Add stereo recording; source-specific notes appear, output channels retain original balance | NOT VERIFIED |
| Silence/multiple notes | Unvoiced spans remain empty; stable notes produce separate blocks | NOT VERIFIED |
| Vibrato/portamento | Curves preserve expressive changes; sustained vibrato does not fragment excessively | NOT VERIFIED |
| Source rate | Repeat with 44.1, 48 and 96 kHz media; reference pitches and time placement agree | NOT VERIFIED |
| Nonzero timeline | Place item at 12 seconds; notes start with that item rather than at zero | NOT VERIFIED |
| Source offset | Slip item to source offset 3 seconds; displayed curve matches the audible excerpt | NOT VERIFIED |
| Long source/short item | Trim a longer source to a 2-second item; show only that source interval | NOT VERIFIED |
| Move | Move item from 12 to 25 seconds; Fit shows notes at the new timeline location | NOT VERIFIED |
| Trim | Trim both edges repeatedly; curves and note blocks clip to new boundaries | NOT VERIFIED |
| Split | Split during a note and during silence; both items display their correct source intervals | NOT VERIFIED |
| Multiple sources | Select different takes; no cross-source notes or stale curves | NOT VERIFIED |
| Multiple instances | Use two instances on different recordings; each displays its own sources; delete one while the other analyzes | NOT VERIFIED |
| Navigation | Wheel scroll time, Shift-wheel scroll pitch; Ctrl-wheel time zoom, Ctrl-Shift-wheel pitch zoom; Fit restores view | NOT VERIFIED |
| Selection | Click blocks; highlight and cents/frequency details appear; audio pitch does not change | NOT VERIFIED |
| Unity audio | Equal-rate dry and plugin renders null at unity with identical host routing; no demo dimming on double-click | NOT VERIFIED |
| Different project rate | Use 96 kHz source in 48 kHz project; pitches/timing agree; assess linear-conversion sound quality separately | NOT VERIFIED |
| Seek/cache misses | Start playback at several distant source offsets, loop and seek rapidly; record any fallback/dropouts and cache status | NOT VERIFIED |
| Region overlaps | Overlap items reading distant positions of one source; verify mixing, steady playback and correct curves | NOT VERIFIED |
| Unsupported stretch/rate | Apply take rate/stretch changes; host either disallows the unsupported transform or editor says unavailable; no misleading aligned curve | NOT VERIFIED |
| Cancel/replace | Start analysis on long media, replace/remove it immediately; no stale results or crash | NOT VERIFIED |
| Close/reopen editor | Close editor during analysis, then reopen; cached result/progress is valid | NOT VERIFIED |
| Save/reopen project | Save, quit REAPER, reopen; sources reanalyze and results align; no crash | NOT VERIFIED |
| Undo/redo | Remove/restore items and replace sources with undo/redo while analysis runs; no stale results/crash | NOT VERIFIED |
| Shutdown | Close project and quit REAPER while several sources analyze; clean worker shutdown | NOT VERIFIED |

## Audio null check

At matching source/project rates, render the same excerpt with and without
PitchGarden using identical settings and no dither. Import both at exactly the
same position, invert one polarity, and sum. Expect cancellation to numerical
precision. Investigate residuals, fades, item gain, channel routing and missing
blocks before attributing them to pitch analysis. Repeat with a source offset,
trimmed boundaries, and stereo material. Capture any realtime seek dropout too;
an offline render alone does not test realtime cache behavior.

Different-rate rendering necessarily resamples. This implementation uses linear
interpolation and is not expected to null against REAPER's band-limited converter.
No pitch correction or gain changes should occur, but converter quality remains
a limitation. For unavailable reads/transforms the plugin retains host input;
REAPER must be checked to determine what input it supplies in those cases.

## Passing the gate

Only change a row to PASSED after carrying it out. Save the tested project and
note source files, sample rates, offsets, plugin commit and any limitations.
If REAPER supplies no audio to ARA or any lifecycle check crashes, capture the
exact steps and logs; do not mark milestone 0.2 complete.
