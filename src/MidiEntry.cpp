#include "PitchGardenARA.h"
class PitchGardenMidiDocumentController final : public PitchGardenDocumentController {
public:
    using PitchGardenDocumentController::PitchGardenDocumentController;
};
class PitchGardenMidiProcessor final : public PitchGardenProcessorBase {
public:
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PitchGardenMidiProcessor(); }
const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory() {
    return juce::ARADocumentControllerSpecialisation::createARAFactory<PitchGardenMidiDocumentController>();
}
