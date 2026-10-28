#pragma once
#include "PitchGardenARA.h"
#include "MidiReaperHost.h"
class PitchGardenMidiDocumentController final : public PitchGardenDocumentController {
public:
    using PitchGardenDocumentController::PitchGardenDocumentController;
};
class PitchGardenMidiProcessor final : public PitchGardenProcessorBase {
public:
    pg::MidiReaperHost midiHost;
    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return &midiHost; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
};
