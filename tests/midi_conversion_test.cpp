#include "MidiConversion.h"
#include <cmath>
#include <iostream>
#include <limits>
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; return 1; } } while(false)
pg::Note note(double start,double end,int pitch) { return {start,end,pitch,double(pitch),0,.99,0,1}; }
int main() {
    const pg::RegionMapping region{12,3,5,5};
    auto result=pg::convertMidiNotes({note(4,5,60)},region);
    CHECK(result.error.empty() && result.notes.size()==1 && result.notes[0].start==1 && result.notes[0].end==2);
    auto events=pg::midiEvents(result.notes,pg::MidiTempo{120});
    CHECK(events.size()==2 && events[0].on && !events[1].on);
    CHECK(events[0].tick==1920 && events[1].tick==3840 && events[0].pitch==60 && events[0].velocity==100 && events[0].channel==1);
    result=pg::convertMidiNotes({note(1,2,60),note(2,4,61),note(7,9,62),note(8,9,63)},region);
    CHECK(result.notes.size()==2 && result.notes[0].start==0 && result.notes[0].end==1 && result.notes[1].start==4 && result.notes[1].end==5);
    result=pg::convertMidiNotes({note(3,5,60),note(4,6,60),note(6,7,60)},region);
    CHECK(result.notes.size()==3 && result.notes[0].end==1 && result.notes[1].start==1);
    events=pg::midiEvents(result.notes,pg::MidiTempo{120});
    CHECK(events.size()==6 && !events[1].on && events[2].on && events[1].tick==events[2].tick);
    result=pg::convertMidiNotes({note(3,4,60),note(3,5,60)},region);
    CHECK(result.notes.size()==1 && result.notes[0].end==2);
    result=pg::convertMidiNotes({note(3,4,-1),note(3,4,128),note(4,4,60),note(5,4,60),note(-1,4,60),note(3,std::numeric_limits<double>::infinity(),60)},region);
    CHECK(result.notes.empty() && result.rejected==6);
    CHECK(pg::convertMidiNotes({},region).notes.empty());
    CHECK(!pg::convertMidiNotes({note(3,4,60)},{12,3,5,6}).error.empty());
    for(double bpm:{60.,90.,120.,173.,240.}) {
        pg::MidiTempo tempo{bpm};
        for(double seconds:{0.,.1234,1.,7.391,100.}) {
            auto ticks=tempo.toTicks(seconds); CHECK(ticks>=0);
            CHECK(std::abs(tempo.toSeconds(ticks)-seconds)<=tempo.toSeconds(1)/2+1e-10);
        }
    }
    bool invalid=false; try { pg::MidiTempo{0}.toTicks(1); } catch(...) { invalid=true; } CHECK(invalid);
    events=pg::midiEvents({{60,0,.00001}},pg::MidiTempo{120}); CHECK(events.empty());
    result=pg::convertMidiNotes({note(3,4,0),note(4,5,127)},region);
    for(const auto& e:pg::midiEvents(result.notes,pg::MidiTempo{120})) CHECK(e.pitch>=0 && e.pitch<=127);
    std::cout << "MIDI conversion, clipping, overlap ordering, validation and explicit-tempo round trip passed\n";
}
