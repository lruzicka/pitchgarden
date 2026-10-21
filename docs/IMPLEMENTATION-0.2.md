# Milestone 0.2 implementation

The supplied milestone is the design brief. Work in the existing clean checkout on
feature/pitch-analysis-0.2. Preserve plugin IDs and dependency gitlinks.

A. Add validated frequency/time conversion and incremental offline analysis using
   PitchDetector, and a JUCE ARA source-reader adapter. Test sample rates, invalid
   input, timestamps and source offsets. Link the core into the plugin; build.
B. Add a cancellable source-owned analysis job publishing immutable snapshots.
   Construct/destroy readers on the document thread, read only through JUCE's
   synchronized reader on workers. Stop before model invalidation/destruction.
   Test cancellation, replacement, errors and shutdown; build.
C. Segment confident frames with median estimates, minimum duration, gap tolerance
   and persistent pitch transitions. Preserve frame indices and raw observations.
   Test stable notes, transitions, vibrato, silence and gaps; build.
D. Add a custom piano roll using source results and current region mappings,
   clip source offsets/trims, expose status, pan/zoom and selection. Reject unsupported
   time transformations visibly. Preserve unity playback and the plugin identity.
E. Run the complete test suite and VST3 build, document exact dependency revisions,
   limitations and reproducible Fedora/REAPER acceptance checks. Interactive host
   checks remain NOT VERIFIED unless actually performed.

Each completed phase receives a separate commit. No push. No correction DSP.

## Inspection and decisions

- Initial checkout clean, master at cb63579; no user edits to preserve.
- JUCE gitlink 454fe891acc8204f99f4ef1e3a28c84b1a4da145 is JUCE 9.0.4,
  not the README's claimed 8.0.10. Preserve the existing pin; JUCE 8 is unverified.
- ARA SDK a2b1aac1d1d5c4eed387db85a9c0cdb7d460254c; nested API
  65ec5c43b943a48cb5446f448a0492db6af8534b and Library
  d18a6a5e489816316be84a9de0eaf7307bc1abe4 restored from local Git objects
  because network DNS is unavailable. No third-party source edits.
- The sample's renderer rejects source/host rate or channel mismatches; the
  sample's preview renderer also documents incorrect rate conversion. These
  cannot be advertised as working inherited behavior.
- ARAInterface.h Audio Access Controller rules require reader creation/destruction
  in source-management calls or endEditing, not arbitrary GUI timer callbacks.
- Analysis window increases to at least 2*sampleRate/65+2 samples to retain the
  configured minimum at high sample rates. Window/hop remain sample counts.

Phase A: core CTest 2/2 passed; full pinned VST3 build passed. Commit 525e252.
Phase B: per-source background jobs, immutable snapshots, clearing on invalidation,
reader shutdown before source edits, and endEditing reader recreation implemented.
Core CTest 3/3 and full incremental VST3 build passed. The temporary generic editor
will be replaced in D. The original demo supplies playback until D.
