# PitchGarden MIDI implementation plan

Use the user's existing development branch and preserve original plugin behavior.

- [x] A: Add independent MIDI VST3 identity/version/factory/archive IDs; derive
  from the existing processor/controller and reuse source analysis without DSP
  copies. Build both targets and run original tests, then commit.
- [x] B: Add independent region-clipped note conversion, deterministic overlap
  handling, explicit constant tempo and 960-PPQ ticks. Use JUCE SMF writer and
  reader round-trip tests. Commit after tests.
- [x] C: Add focused MIDI editor with immutable single-region snapshots,
  compact read-only note view, status and asynchronous Export MIDI chooser.
  Add ARA integration checks, then commit.
- [x] D: Prepare unique retained temporary MIDI files before external X11 drag.
  Gate direct drag on verified constant project tempo. Add native selected-track
  insertion using REAPER project seconds-to-PPQ as the tempo-changing fallback.
  Test API flags, failure cleanup, project lifetimes, and nonlinear timing. Commit.
- [x] E: Build both plugins, run all regressions/identity checks, document actual
  Linux capabilities and unverified REAPER matrix, add Fedora installation
  script/instructions, install both bundles safely, and commit. Never push.

Portable files use a documented explicit constant tempo (verified host tempo
when constant, otherwise 120 BPM). Drag into a changing/unknown host tempo is
not advertised as timing-safe. Native insertion maps each boundary through
MIDI_GetPPQPosFromProjTime and does not alter the tempo map or source item.

No native MIDI editor, pitch correction, quantization, instrument playback,
SDK update, global input simulation or audio-callback file I/O is added.

Automated implementation/build/install phases are finished. Live REAPER acceptance
remains UNVERIFIED and the definition-of-done host gate remains open. See
`docs/REAPER-MIDI-ACCEPTANCE-0.1.md`. No push was performed.
