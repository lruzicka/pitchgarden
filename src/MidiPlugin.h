#pragma once
#include "PitchGardenARA.h"
class PitchGardenMidiDocumentController final : public PitchGardenDocumentController {
public:
    using PitchGardenDocumentController::PitchGardenDocumentController;
};
class PitchGardenMidiProcessor final : public PitchGardenProcessorBase {
public:
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
};
