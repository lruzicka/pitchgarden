#pragma once
#include "NoteSegmenter.h"
#include "PitchAnalysis.h"
#include <cstdint>
#include <string>
namespace pg {
inline constexpr int midiPPQ=960;
struct MidiNote { int pitch=0; double start=0,end=0; }; // region-relative seconds
struct MidiConversion {
    std::vector<MidiNote> notes;
    std::size_t rejected=0,overlaps=0;
    std::string error;
};
// Invalid pitches are rejected, never clamped. Equal-pitch overlaps are cut at
// the next onset; identical onsets coalesce to the longest duration.
MidiConversion convertMidiNotes(const std::vector<Note>&,const RegionMapping&);
struct MidiTempo {
    double bpm=120;
    int microseconds() const;
    std::int64_t toTicks(double seconds) const;
    double toSeconds(std::int64_t ticks) const;
};
struct MidiEvent { std::int64_t tick=0; int pitch=0; bool on=false; int velocity=100,channel=1; };
// Note Off precedes Note On at equal timestamps, then ascending pitch.
// Notes that collapse to zero ticks are omitted rather than stretched.
std::vector<MidiEvent> midiEvents(const std::vector<MidiNote>&,MidiTempo);
}
