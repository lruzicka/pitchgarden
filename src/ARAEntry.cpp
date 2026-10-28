// Analysis-only ARA integration. Plugin identity remains compatible with 0.1.
#include "JuceHeader.h"
#include "PitchGardenARA.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchGardenProcessor();
}

#if JucePlugin_Enable_ARA
const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<PitchGardenDocumentController>();
}
#endif
