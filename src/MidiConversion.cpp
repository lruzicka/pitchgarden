#include "MidiConversion.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace pg {
MidiConversion convertMidiNotes(const std::vector<Note>& notes,const RegionMapping& mapping) {
    MidiConversion result;
    if(!mapping.supported() || !std::isfinite(mapping.sourceStart+mapping.sourceDuration)
        || !std::isfinite(mapping.timelineStart+mapping.playbackDuration)) {
        result.error="Select one un-stretched playback region with valid source timing"; return result;
    }
    std::vector<MidiNote> clipped;
    for(const auto& note:notes) {
        if(note.midiNote<0 || note.midiNote>127 || !std::isfinite(note.continuousMidi)
            || !std::isfinite(note.start) || !std::isfinite(note.end) || note.start<0 || note.end<=note.start) {
            ++result.rejected; continue;
        }
        const auto begin=std::max(note.start,mapping.sourceStart);
        const auto end=std::min(note.end,mapping.sourceStart+mapping.sourceDuration);
        if(end<=begin) continue; // trimmed away, not an invalid detection
        clipped.push_back({note.midiNote,begin-mapping.sourceStart,end-mapping.sourceStart});
    }
    std::sort(clipped.begin(),clipped.end(),[](const auto& a,const auto& b) {
        if(a.pitch!=b.pitch) return a.pitch<b.pitch;
        if(a.start!=b.start) return a.start<b.start;
        return a.end<b.end;
    });
    for(const auto& next:clipped) {
        if(!result.notes.empty() && result.notes.back().pitch==next.pitch) {
            auto& prior=result.notes.back();
            if(prior.start==next.start) { prior.end=std::max(prior.end,next.end); ++result.overlaps; continue; }
            if(prior.end>next.start) { prior.end=next.start; ++result.overlaps; }
        }
        result.notes.push_back(next);
    }
    std::sort(result.notes.begin(),result.notes.end(),[](const auto& a,const auto& b) {
        return a.start!=b.start ? a.start<b.start : a.pitch<b.pitch;
    });
    return result;
}
int MidiTempo::microseconds() const {
    if(!std::isfinite(bpm) || bpm<=0 || 60000000./bpm<1 || 60000000./bpm>16777215)
        throw std::invalid_argument("MIDI tempo is out of range");
    return static_cast<int>(std::llround(60000000./bpm));
}
std::int64_t MidiTempo::toTicks(double seconds) const {
    const auto ticks=seconds*1000000./microseconds()*midiPPQ;
    if(!std::isfinite(seconds) || seconds<0 || !std::isfinite(ticks) || ticks>0x0fffffff)
        throw std::invalid_argument("MIDI duration is out of range");
    return std::llround(ticks);
}
double MidiTempo::toSeconds(std::int64_t ticks) const { return double(ticks)*microseconds()/1000000./midiPPQ; }
std::vector<MidiEvent> midiEvents(const std::vector<MidiNote>& notes,MidiTempo tempo) {
    std::vector<MidiEvent> events;
    // Reuse normalization in tick space too: rounding can merge two onsets.
    std::vector<Note> tickNotes;
    double last=0;
    for(const auto& note:notes) {
        if(note.pitch<0 || note.pitch>127 || !std::isfinite(note.start) || !std::isfinite(note.end)
            || note.start<0 || note.end<=note.start) continue;
        const auto start=tempo.toTicks(note.start),end=tempo.toTicks(note.end);
        if(end<=start) continue;
        tickNotes.push_back({double(start),double(end),note.pitch,double(note.pitch),0,1,0,1}); last=std::max(last,double(end));
    }
    if(tickNotes.empty()) return events;
    for(const auto& note:convertMidiNotes(tickNotes,{0,0,last,last}).notes) {
        events.push_back({static_cast<std::int64_t>(note.start),note.pitch,true,100,1});
        events.push_back({static_cast<std::int64_t>(note.end),note.pitch,false,100,1});
    }
    std::sort(events.begin(),events.end(),[](const auto& a,const auto& b) {
        if(a.tick!=b.tick) return a.tick<b.tick;
        if(a.on!=b.on) return !a.on;
        return a.pitch<b.pitch;
    });
    return events;
}
}
