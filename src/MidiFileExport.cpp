#include "MidiFileExport.h"
namespace pg {
juce::Result midiFileBytes(const std::vector<MidiNote>& notes,double duration,MidiTempo tempo,juce::MemoryBlock& bytes) {
    bytes.reset();
    try {
        const auto end=tempo.toTicks(duration);
        const auto events=midiEvents(notes,tempo);
        if(events.empty()) return juce::Result::fail("No valid MIDI notes to export");
        if(end<=0 || events.back().tick>end) return juce::Result::fail("MIDI notes exceed the selected region");
        juce::MidiMessageSequence track;
        auto signature=juce::MidiMessage::tempoMetaEvent(tempo.microseconds()); signature.setTimeStamp(0); track.addEvent(signature);
        for(const auto& event:events) {
            auto message=event.on ? juce::MidiMessage::noteOn(event.channel,event.pitch,static_cast<juce::uint8>(100))
                                  : juce::MidiMessage::noteOff(event.channel,event.pitch);
            message.setTimeStamp(double(event.tick)); track.addEvent(message);
        }
        auto finish=juce::MidiMessage::endOfTrack(); finish.setTimeStamp(double(end)); track.addEvent(finish);
        juce::MidiFile file; file.setTicksPerQuarterNote(midiPPQ); file.addTrack(track);
        juce::MemoryOutputStream stream(bytes,false);
        if(!file.writeTo(stream,0)) { bytes.reset(); return juce::Result::fail("Could not encode MIDI file"); }
        return juce::Result::ok();
    } catch(const std::exception& e) { return juce::Result::fail(e.what()); }
}
juce::Result writeMidiFile(const juce::File& destination,const std::vector<MidiNote>& notes,double duration,MidiTempo tempo) {
    juce::MemoryBlock bytes;
    const auto encoded=midiFileBytes(notes,duration,tempo,bytes); if(encoded.failed()) return encoded;
    juce::TemporaryFile temporary(destination);
    if(!temporary.getFile().replaceWithData(bytes.getData(),bytes.getSize()) || !temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not write MIDI file: "+destination.getFullPathName());
    return juce::Result::ok();
}
}
