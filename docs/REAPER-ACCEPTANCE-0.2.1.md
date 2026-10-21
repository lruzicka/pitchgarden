# PitchGarden 0.2.1 — native REAPER / Fedora acceptance

**All live REAPER checks below: NOT VERIFIED by this implementation session.**
The user reports that the earlier 0.2 note display, selection, scrolling and zoom
work. This checklist covers the new transport, focus, audition and uncertainty
behavior. Automated in-process ARA tests do not certify REAPER compatibility.

## Setup

1. Run `scripts/build.sh`, quit REAPER, and retain a copy of the installed 0.2
   bundle before replacing it with
   `build/PitchGardenARA_artefacts/Release/VST3/PitchGarden.vst3` in `~/.vst3/`.
   Start REAPER and rescan if required. Record REAPER/Fedora versions and commit.
2. Open a disposable ARA project with a real mono vocal, stereo audio, silence,
   stable notes, vibrato and pitch slides. Include 44.1, 48 and 96 kHz sources.
3. Put an item at project time 12 seconds with a nonzero source offset (e.g.
   3 seconds), then open its ARA editor. Keep another plugin instance on a
   different source to check instance isolation.
4. Inspect REAPER's per-FX keyboard routing for both floating and embedded
   editors. Record the setting; avoid assuming that wording or defaults are the
   same across REAPER versions. Do not enable global keyboard interception.
5. Keep an equal-rate dry reference render. Reuse the baseline audio null checks
   in `REAPER-ACCEPTANCE.md` after the transport/editor tests.

## Transport and navigation

| Check | Expected behavior | Status |
| --- | --- | --- |
| Playback cursor | Yellow cursor tracks REAPER project seconds over the audible recording; source offset is not applied twice | NOT VERIFIED |
| Stop | Cursor turns orange and shows the host-reported stopped position; it does not continue running | NOT VERIFIED |
| Seek during playback | Cursor jumps promptly to the new host position; pitch data stays in place | NOT VERIFIED |
| Seek while stopped | Cursor reflects new positions if REAPER continues processor callbacks; record any stopped/suspended-callback limitation | NOT VERIFIED |
| Loop | Loop a short interval crossing note boundaries; cursor wraps to the host's loop start, without extrapolated drift | NOT VERIFIED |
| Follow off | Start with Follow off, seek/loop/play outside the visible interval; viewport stays where manually placed | NOT VERIFIED |
| Follow on | Enable Follow and play past the visible right margin; viewport follows; seeking/looping backward follows actual project time | NOT VERIFIED |
| Manual navigation | Scroll time or pitch, zoom either axis, or press Fit; Follow turns off and the next callback does not undo navigation | NOT VERIFIED |
| Re-enable Follow | Explicitly enable Follow again; following resumes during playback | NOT VERIFIED |
| Source/timeline mapping | At timeline 13 s with source offset 3 s and item start 12 s, cursor overlays source time 4 s | NOT VERIFIED |
| Move/trim/split | Move item to 25 s, trim both edges and split it; notes and audition ranges follow the current mapping, cursor remains in project time | NOT VERIFIED |
| Project sample rate | Repeat with 96 kHz media in a 48 kHz project; cursor position agrees with the host ruler | NOT VERIFIED |

## Space and focus

Test floating and embedded editors separately, with REAPER's keyboard-routing
configuration recorded. There must be exactly one play/stop change per press.

| Check | Expected behavior | Status |
| --- | --- | --- |
| Click note, then Space | REAPER starts/stops; no pitch change or note drag | NOT VERIFIED |
| Click Fit/Follow, then Space | Host playback changes, not a repeated button action | NOT VERIFIED |
| Click Audition/Stop preview, then Space | Host transport responds once; preview cancels if host playback starts | NOT VERIFIED |
| Hold Space | No rapid repeated toggling from auto-repeat | NOT VERIFIED |
| Other keys/modifiers | Unhandled keys and modified Space are not claimed by PitchGarden | NOT VERIFIED |
| Focus changes | Alternate main REAPER window, editor and another plugin; no stuck Space latch | NOT VERIFIED |
| Missing controller | If REAPER provides no ARA playback-controller interface, native host routing must handle Space; note the configuration needed | NOT VERIFIED |

Implementation uses JUCE no-grab settings and optional ARA message-thread
play/stop requests. It sends no global key events. ARA permits the host to ignore
requests, so a test failure must record role/interface availability and routing
rather than claiming that issuing a request proves playback changed.

## Original-note audition

First inspect the audition status. The host must assign this plugin the ARA
editor-renderer role, assign the chosen region/sequence, and schedule stopped
realtime render callbacks. These are separate requirements.

| Check | Expected behavior | Status |
| --- | --- | --- |
| Capability | Absent role reports unavailable; unassigned region reports that condition; never silently starts normal transport | NOT VERIFIED |
| Stopped callbacks | If requested preview remains queued for over a second without callbacks, status reports that stopped preview may be unavailable | NOT VERIFIED |
| Original mono note | Select a note and press Audition; hear its original source interval once, with only short preview boundary fades | NOT VERIFIED |
| Stereo/rates | Repeat with stereo and 96 kHz media at another project rate; pitch/timing and channel relationship remain correct | NOT VERIFIED |
| Source offset/trim | Audition a partially trimmed note; only the note portion inside the item is previewed | NOT VERIFIED |
| Host transport unchanged | Audition does not seek, start tracks, change repeat/loop ranges, or alter the project edit cursor | NOT VERIFIED |
| Stop preview | Preview stops, status returns idle, and no later callback resumes it | NOT VERIFIED |
| Host starts playing | Preview cancels immediately when playback starts; track output remains unchanged | NOT VERIFIED |
| Item/source changes | Move/trim/delete/replace during preview; old request cancels safely | NOT VERIFIED |
| Editor close | Close while queued or playing, then reopen; no preview continues or resumes | NOT VERIFIED |
| Assignment/multiple instances | Switch selected items/instances; preview is confined to the instance assigned that region | NOT VERIFIED |
| Offline export | Export with a queued preview; no audition audio is added to the render | NOT VERIFIED |

ARA defines no request to wake a suspended editor renderer. If REAPER provides
only a playback-renderer role or does not schedule editor callbacks while stopped,
this preview path cannot produce sound. Document the observed limitation; do not
work around it by starting/seeking track playback or opening an unrelated audio
device. Role presence alone must not be recorded as a successful audition test.

## Pitch uncertainty and regressions

| Check | Expected behavior | Status |
| --- | --- | --- |
| Brief octave spikes | Short out-and-back octave-like observations may appear as orange dots; raw pitch remains at its detected height | NOT VERIFIED |
| Sustained octave | A genuine sustained octave change remains a separate note/trajectory | NOT VERIFIED |
| Vibrato/portamento | Expressive curves remain intact; no semitone quantization or octave folding | NOT VERIFIED |
| Low confidence | Uncertain voiced observations appear grey rather than a misleading solid white connection | NOT VERIFIED |
| Baseline audio | Matching-rate dry render still nulls; note selection, Follow and Space do not introduce gain/pitch changes | NOT VERIFIED |
| Save/reopen/lifecycle | Save/reopen project and close during analysis/preview; no stale data or crash | NOT VERIFIED |

Synthetic investigation demonstrates YIN harmonic ambiguity even at high
confidence; it does not establish that any particular orange point in a vocal
is an error. Intentional short octave ornaments may be marked uncertain, and
persistent octave mistakes remain unresolved. Save problematic vocal excerpts
and the tested project for future detector evaluation.

Record actual outcomes and limitations before changing a row to PASSED. The
milestone's live acceptance gate stays open until transport and keyboard behavior
are demonstrated in REAPER and audition capability or its observed host limitation
is recorded.
