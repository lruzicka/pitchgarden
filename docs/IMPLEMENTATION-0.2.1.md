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

Audition: optional ARA editor renderer adds cached original source audio only in
stopped realtime rendering. Assigned-region identity, source revision and exact
region mapping are validated; no source pointers in requests. New per-renderer
assignment gates cover ARA assignment callbacks outside editing cycles. Streamed
preview has 5 ms boundary fades and stops at the clipped source note range. Host
playback, source invalidation, item edits or editor close cancel preview. No host
seek/start/stop/cycle calls are used for audition. GUI reports absent role,
unassigned region, cache wait and missing stopped render callbacks. No claim
that REAPER provides or schedules this optional role. Integration checks add-only
original samples, preview finish/stop, no export preview, host-play cancellation,
movement/invalidation and unchanged host transport request counts. Full build
and CTest 8/8 passed. The test host's known-role mask must include all three ARA
roles; unknown roles are treated as enabled for backward compatibility.

Octave investigation: deterministic 110 Hz fundamental + 220 Hz second harmonic
at amplitudes 0.01/0.8 yields approximately 220.004 Hz with 0.99967 confidence;
raising the fundamental to 0.4 yields 110 Hz. YIN confidence alone cannot establish
the intended fundamental. No user vocal recording was available for diagnosis.
Added value-only quality annotations: reliable, unvoiced, low confidence, and
brief octave excursion. A return within 80 ms to a confident stable neighborhood
is required, with 0.65-semitone octave tolerance. No correction/filter rewrites
raw frequency, confidence, MIDI or note segmentation. Suspect points remain at
their detected pitch as orange dots; low-confidence points are grey; no misleading
continuous line bridges octave-like discontinuities. Sustained octave notes,
vibrato and continuous slides remain intact. Failing stub test observed before
implementation; core tests 8/8 and full tests 9/9 passed; full VST3 build passed.
