#include "ARAAnalysisBridge.h"
#include <stdexcept>
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
    for (int i=0;i<count;++i) {
        double sum=0;
        for (int c=0;c<channels;++c) sum+=buffer.getSample(c,i);
        dst[i]=static_cast<float>(sum/channels);
    }
    return true;
}

std::vector<pg::PitchFrame> ARAAnalysisBridge::analyze(const std::atomic<bool>* cancelled,
    const std::function<void(float)>& progress) {
    return pg::analyzeSource(sampleRate,sampleCount,
        [this](std::int64_t pos,int n,float* dst) { return readMono(pos,n,dst); },
        {},cancelled,progress);
}
