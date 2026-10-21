// Temporary, functional ARA integration scaffold using JUCE's ISC-licensed demo.
// No pitch editing is wired to the playback renderer yet.
#include "JuceHeader.h"
#include "ARAPluginDemo.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ARADemoPluginAudioProcessor();
}

#if JucePlugin_Enable_ARA
const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<ARADemoPluginDocumentControllerSpecialisation>();
}
#endif
