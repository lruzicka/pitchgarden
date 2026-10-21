# PitchGarden 0.2.1 stabilization

The user-provided requirements authorize incremental implementation on the clean
0.2 checkout. Preserve pinned SDKs, IDs and analysis-only dry playback.

1. Transport and focus: inspect actual APIs; publish bounded atomic host position
   snapshots from processBlock; draw a project-time cursor; optional Follow pauses
   when navigating manually. Use native JUCE focus settings and optional ARA
   play/stop requests for Space, never global hooks. Test pure mapping/state and
   integration. Commit.
2. Original-note audition: implement the optional ARA editor-renderer path only;
   cache reads must remain nonblocking. Use value-only region/revision requests,
   assigned-region validation and cancellation for changes, host playback and
   editor close. Report absent role or missing stopped callbacks honestly. Test
   original samples, role gating, lifecycle, source offsets and no track changes.
   Commit.
3. Octave uncertainty: reproduce YIN harmonic ambiguity with deterministic data;
   mark brief octave-like excursions and low-confidence frames without rewriting
   raw observations or smoothing vibrato/slides. Keep sustained octave notes.
   Test and commit.
4. Full build/tests, independent review, version/docs and manual REAPER procedure.
   Commit and leave the branch unpushed. REAPER acceptance remains NOT VERIFIED
   unless an actual interactive session can be exercised.

Inspected pins: JUCE 454fe891acc8204f99f4ef1e3a28c84b1a4da145 (9.0.4+1),
ARA SDK a2b1aac1d1d5c4eed387db85a9c0cdb7d460254c, API 65ec5c4,
Library d18a6a5. No dependency upgrade or third-party source edit planned.

Transport/focus: audio callback publishes actual host project position once per
block; GUI polls bounded atomic snapshots at 30 Hz and repaints cursor strips.
No extrapolation or GUI-thread AudioPlayHead access. Stopped seeks can only update
when the host supplies callbacks. Follow defaults off and suspends on wheel/Fit.
Explicit no-keyboard-grab settings plus optional message-thread ARA Space requests
with repeat suppression. Actual JUCE VST3 Linux wrapper does not implement a
custom key forwarding override; no platform event injection is introduced.
Full build and CTest 8/8 passed, including fake host Space requests and transport.
