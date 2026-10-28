#include "MidiPlugin.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PitchGardenMidiProcessor(); }
const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory() {
    return juce::ARADocumentControllerSpecialisation::createARAFactory<PitchGardenMidiDocumentController>();
}
