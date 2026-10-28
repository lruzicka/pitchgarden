#pragma once
#include "JuceHeader.h"
#include "MidiConversion.h"
namespace pg {
juce::Result midiFileBytes(const std::vector<MidiNote>&,double duration,MidiTempo,juce::MemoryBlock&);
juce::Result writeMidiFile(const juce::File&,const std::vector<MidiNote>&,double duration,MidiTempo);
}
