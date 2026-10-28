#include "MidiFileExport.h"
#include <iostream>
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; return 1; } } while(false)
int main() {
    const std::vector<pg::MidiNote> notes{{60,.1234,.4213},{60,.4213,.9},{67,1.2,1.9}};
    for(double bpm:{60.,120.,173.}) {
        juce::MemoryBlock bytes;
        CHECK(pg::midiFileBytes(notes,2,pg::MidiTempo{bpm},bytes).wasOk());
        CHECK(bytes.getSize()>14 && std::memcmp(bytes.getData(),"MThd",4)==0);
        juce::MemoryInputStream stream(bytes,false); juce::MidiFile file;
        CHECK(file.readFrom(stream) && file.getTimeFormat()==960 && file.getNumTracks()==1);
        file.convertTimestampTicksToSeconds();
        int on=0,off=0; std::vector<double> starts,ends;
        for(int i=0;i<file.getTrack(0)->getNumEvents();++i) {
            const auto& message=file.getTrack(0)->getEventPointer(i)->message;
            if(message.isNoteOn()) { CHECK(message.getChannel()==1 && message.getVelocity()==100); CHECK(message.getNoteNumber()==notes[std::size_t(on)].pitch); starts.push_back(message.getTimeStamp()); ++on; }
            if(message.isNoteOff()) { ends.push_back(message.getTimeStamp()); ++off; }
        }
        CHECK(on==3 && off==3);
        for(std::size_t i=0;i<notes.size();++i) {
            const auto tolerance=pg::MidiTempo{bpm}.toSeconds(1)/2+1e-8;
            CHECK(std::abs(starts[i]-notes[i].start)<tolerance && std::abs(ends[i]-notes[i].end)<tolerance);
        }
    }
    juce::MemoryBlock empty; CHECK(pg::midiFileBytes({},1,{120},empty).failed() && empty.isEmpty());
    CHECK(pg::midiFileBytes({{130,0,1}},1,{120},empty).failed() && empty.isEmpty());
    auto target=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("pitchgarden-midi-file-test",".mid");
    CHECK(pg::writeMidiFile(target,notes,2,{120}).wasOk()); CHECK(target.existsAsFile());
    const auto prior=target.getSize(); CHECK(pg::writeMidiFile(target,{},2,{120}).failed() && target.getSize()==prior);
    target.deleteFile();
    std::cout << "SMF type 0 header, tempo, balanced notes, round trip, atomic file and empty rejection passed\n";
}
