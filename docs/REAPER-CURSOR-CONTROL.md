# REAPER edit cursor control

Acceptance is pending independent live REAPER checks of cursor movement AND note
audition. A successful cursor test alone does not close this gate.

## Chosen host mechanism

Use JUCE's `VST3ClientExtensions::setIHostApplication` to query Cockos'
[IReaperHostApplication](https://github.com/justinfrankel/reaper-sdk/blob/main/sdk/reaper_vst3_interfaces.h).
It supplies the public REAPER API and the instance's parent project (context 3).
Call [SetEditCurPos2](https://www.reaper.fm/sdk/reascript/reascripthelp.html#SetEditCurPos2)
with that project, project seconds, `moveview=false`, and `seekplay=false`.
Use `ValidatePtr(project,"ReaProject*")` for each UI operation and optionally read
`GetCursorPositionEx` to draw the actual edit cursor independently of playback.
No project pointer is cached or replaced with the active tab. No main transport
start, stop, seek or ARA preview request occurs on this path.

ARA `requestSetPlaybackPosition` is a playback-position request; it does not
specify a separate edit cursor or expose a no-playback-seek option. Ordinary
VST3 provides no equivalent project edit-cursor API. The supported REAPER VST3
extension avoids a separate ReaScript/OSC bridge. If the extension, project or
required API is absent, the cursor gesture reports unavailable and audition
continues to work. No global input simulation or SDK change is used.

Host context replacement is synchronized outside audio processing. GUI calls
retain a snapshot of the queried interface; its COM reference is released when
that binding is replaced or the processor is destroyed. API calls are restricted
to JUCE's message thread. The audio callback and audition renderer are untouched.

## Mouse interaction and time mapping

- Note hit testing runs first in the plot. Left press selects and starts the
  existing recorded-source audition; drag keeps that gesture, including dragging
  over the ruler. Release stops audition through its existing path.
- Empty plot left press moves the edit cursor once. Dragging from the background
  does not turn into a ruler gesture or note audition.
- Left press in the time ruler begins continuous edit-cursor movement. Dragging
  across a note remains a cursor gesture; it never starts audition. Release,
  disabling/hiding the editor and source/view invalidation cancel the gesture.
- Controls, pitch labels, footer and right button do not seek. Space retains its
  existing supported ARA start/stop handler and native wrapper listener.

Horizontal pixels already represent project seconds:
`project = viewStart + (x - plotLeft) * viewDuration / plotWidth`.
For a source note, the equivalent mapping is
`project = region.timelineStart + sourceTime - region.sourceStart`.
Source offsets are applied in note drawing/hit testing and audition clipping,
not added a second time to the project-axis click. Example: source offset 3 s,
region project start 12 s, source time 4 s means project time 13 s. Moving or
trimming the item changes the source/project relation, not the axis definition.
Ruler dragging disables Follow to keep that definition stable during the gesture.

## Automated verification scope

CTest `reaper_cursor_api` uses the published VST3 ABI and a deterministic host double
to check function lookup, query/release lifetimes, current project context,
project invalidation, missing APIs, message-thread rejection and both false
flags. `ara_integration` separately exercises real editor note presses through
ARA preview into recorded source samples: initial source position, uninterrupted
continuation while dragging onto the ruler, outside release and held-editor
closure. Mouse callback assertions separately cover blank-click-only behavior,
ruler dragging over notes, outside release, non-target areas, offsets/trims,
missing cursor APIs and audition after cursor gestures. Existing ARA Space
start/stop tests remain. These are automated regressions, not live REAPER proof.

The optional native JUCE Space test also passes with this build. No playback,
source-analysis, source cache or audition renderer implementation was changed.

## Independent live verification

Use a disposable project and the newly built bundle; do not replace a loaded
plugin file. Record REAPER/Fedora/session versions and the plugin commit. Back up
the installed bundle, quit REAPER, install, then restart/rescan.

1. **Audition baseline (independent gate).** Stop REAPER. Hold a note at its
   beginning, middle and near its end. It must start at the same source-relative
   note beginning each time. Drag over blank space/ruler and release outside the
   editor: original-source audio stops, with no transport/cursor move. Repeat
   rapidly, hold beyond note end, close the editor while holding, reopen it, and
   change source/region selection. No overlapping/hanging preview is acceptable.
2. **Cursor (independent gate).** Click blank plot space, verify REAPER's edit
   cursor and the plugin's edit marker agree with the ruler. Drag the ruler
   across and beyond a note; the edit cursor follows continuously, without
   preview or starting main playback. Dragging from blank space must not seek
   continuously. Release outside; further movement must not keep seeking.
3. **Offsets.** Use an item at project 12 s with source offset 3 s. Click the
   project 13 s column: REAPER must show 13 s (source 4 s), not 4, 16 or 10 s.
   Move the item to 30 s, then change its left trim, and repeat. Test two items
   sharing the same source and two project tabs. The plugin must operate on its
   own project, never an unrelated active project tab.
4. **Playback and Space.** Space still starts/stops as before. During playback,
   click empty space/drag the ruler: the edit cursor changes but audible playback
   does not jump. Note holds during stopped transport still audition immediately.
5. **Transparency/lifetime.** Compare ordinary playback to the pre-change build.
   Close/reopen the editor/project and remove/reinsert the FX. Unsupported hosts
   should report missing cursor capability while keeping audition available.

| Gate | Result |
| --- | --- |
| Automated cursor API and project-context tests | PASSED |
| Automated mouse routing and audition regressions | PASSED (real source samples and separate cursor assertions) |
| Full VST3 build and existing suite | PASSED, 11/11 CTests |
| Live REAPER edit cursor including trims/offsets | UNVERIFIED |
| Live REAPER audition independently after cursor gestures | UNVERIFIED |
| Live REAPER Space and no playback seeking | UNVERIFIED |
