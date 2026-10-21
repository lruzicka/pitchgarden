#pragma once
#include "JuceHeader.h"
#include "AnalysisModel.h"
#include "TransportState.h"
#include <functional>

struct PitchRegionDisplay {
    juce::String name;
    pg::RegionMapping mapping;
    pg::AnalysisSnapshot analysis;
    bool supported=true;
    bool playbackUnavailable=false;
};
// Model values only: no host-owned pointers survive the collection callback.
class PitchEditor final : public juce::Component, private juce::Timer {
public:
    explicit PitchEditor(std::function<std::vector<PitchRegionDisplay>()> collect,
        std::function<pg::TransportSnapshot()> transport={});
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
private:
    void timerCallback() override;
    void updateTransport();
    void suspendFollow();
    void fit();
    float x(double time) const;
    float y(double midi) const;
    juce::Rectangle<float> noteBounds(const PitchRegionDisplay&,const pg::Note&) const;
    std::function<std::vector<PitchRegionDisplay>()> collect;
    std::vector<PitchRegionDisplay> regions;
    std::function<pg::TransportSnapshot()> readTransport;
    pg::TransportSnapshot transport;
    juce::ToggleButton followButton{"Follow"};
    juce::TextButton fitButton{"Fit"};
    juce::Label help,status;
    double timeStart=0, timeSpan=10, pitchLow=45, pitchSpan=36;
    int selectedRegion=-1,selectedNote=-1;
    bool fitted=false;
    juce::String detail;
};
