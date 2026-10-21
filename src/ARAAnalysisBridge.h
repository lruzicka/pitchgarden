#pragma once
#include "JuceHeader.h"
#include "PitchAnalysis.h"
// Construct and destroy on the ARA document/message thread. The owner stops its
// worker before destruction or any source model change. Only readMono runs on
// the worker; it uses JUCE's synchronized HostAudioReader wrapper.
class ARAAnalysisBridge {
public:
    explicit ARAAnalysisBridge(juce::ARAAudioSource*);
    bool readMono(std::int64_t start, int count, float* destination);
    std::vector<pg::PitchFrame> analyze(const std::atomic<bool>* cancelled=nullptr,
        const std::function<void(float)>& progress={});
    const double sampleRate;
    const std::int64_t sampleCount;
private:
    juce::ARAAudioSourceReader reader;
    juce::AudioBuffer<float> buffer;
};
