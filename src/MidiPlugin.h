#pragma once
#include "PitchGardenARA.h"
#include "MidiReaperHost.h"
class PitchGardenMidiDocumentController final : public PitchGardenDocumentController {
public:
    using PitchGardenDocumentController::PitchGardenDocumentController;
protected:
    juce::ARAAudioSource* doCreateAudioSource(juce::ARADocument* document,ARA::ARAAudioSourceHostRef ref) noexcept override {
        pg::AnalysisSettings settings; settings.minHz=30; settings.maxHz=2000;
        return new AnalyzedSource(document,ref,settings);
    }
};
class PitchGardenMidiProcessor final : public PitchGardenProcessorBase {
public:
    pg::MidiReaperHost midiHost;
    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return &midiHost; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
};
