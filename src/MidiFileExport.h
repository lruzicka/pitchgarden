#pragma once
#include "JuceHeader.h"
#include "MidiConversion.h"
namespace pg {
juce::Result midiFileBytes(const std::vector<MidiNote>&,double duration,MidiTempo,juce::MemoryBlock&);
juce::Result retainedMidiFile(const std::vector<MidiNote>&,double duration,MidiTempo,juce::File&);
juce::Result writeMidiFile(const juce::File&,const std::vector<MidiNote>&,double duration,MidiTempo);
}
