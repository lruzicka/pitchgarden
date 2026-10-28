#include "ARAAnalysisBridge.h"
#include <stdexcept>
#include <algorithm>
ARAAnalysisBridge::ARAAnalysisBridge(juce::ARAAudioSource* source)
    : sampleRate(source->getSampleRate()), sampleCount(source->getSampleCount()), reader(source) {
    if (source->getChannelCount()<1 || source->getChannelCount()>2)
        throw std::runtime_error("Analysis supports mono and stereo sources");
    pg::AnalysisSettings settings;
    buffer.setSize(source->getChannelCount(),static_cast<int>(settings.windowFor(sampleRate)));
}
bool ARAAnalysisBridge::readMono(std::int64_t start, int count, float* dst) {
    if (count>buffer.getNumSamples() || !reader.read(&buffer,0,count,start,true,true)) return false;
    const auto channels=buffer.getNumChannels();
    // Analyze the highest-energy channel independently. Averaging stereo can
    // cancel the fundamental (or all audio with opposite-polarity channels).
    // This affects analysis only; playback retains every original channel.
    int selected=0; double highest=-1;
    for (int c=0;c<channels;++c) {
        double energy=0;
        for (int i=0;i<count;++i) energy+=double(buffer.getSample(c,i))*buffer.getSample(c,i);
        if (energy>highest) { highest=energy; selected=c; }
    }
    std::copy_n(buffer.getReadPointer(selected),count,dst);
    return true;
}

std::vector<pg::PitchFrame> ARAAnalysisBridge::analyze(const std::atomic<bool>* cancelled,
    const std::function<void(float)>& progress) {
    return pg::analyzeSource(sampleRate,sampleCount,
        [this](std::int64_t pos,int n,float* dst) { return readMono(pos,n,dst); },
        {},cancelled,progress);
}
