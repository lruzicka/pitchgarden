#pragma once
#include "AnalyzedSource.h"
#include "DryPlayback.h"
struct ProcessingLockInterface {
    virtual ~ProcessingLockInterface()=default;
    virtual pg::RenderGate::ReadToken getProcessingLock()=0;
};
class PitchGardenPlayback final : public juce::ARAPlaybackRenderer {
public:
    PitchGardenPlayback(ARA::PlugIn::DocumentController* dc,ProcessingLockInterface& l)
        : ARAPlaybackRenderer(dc),lock(l) {}
    void prepareToPlay(double sr,int maximum,int channels,juce::AudioProcessor::ProcessingPrecision,
                       AlwaysNonRealtime) override {
        rate=sr; maxBlock=maximum;
        rendered.setSize(channels,maximum);
        // Bound all supported source rates without allocation during rendering.
        samples.setSize(2,static_cast<int>(std::ceil(maximum*384000.0/sr))+4);
    }
    void releaseResources() override { samples.setSize(0,0); rendered.setSize(0,0); }
    bool processBlock(juce::AudioBuffer<float>& output,juce::AudioProcessor::Realtime realtime,
                      const juce::AudioPlayHead::PositionInfo& position) noexcept override {
        const auto guard=lock.getProcessingLock();
        if(!guard.isLocked()) return true; // preserve host input during model edits
        const auto count=output.getNumSamples();
        if(count>maxBlock || rate<=0) return true;
        if(!position.getIsPlaying()) { output.clear(); return true; }
        const auto start=position.getTimeInSamples().orFallback(0);
        // Validate the whole block before replacing host audio. Unsupported
        // transformations keep the host signal; the editor reports the reason.
        for(auto* region:getPlaybackRegions()) {
            auto* source=region->getAudioModification()->getAudioSource<AnalyzedSource>();
            if(!mapping(*region).supported() || region->isTimestretchEnabled() || region->isTimeStretchReflectingTempo()
                || source->getSampleRate()>384000
                || source->getChannelCount()<1 || source->getChannelCount()>2) return true;
        }
        rendered.clear();
        for(auto* region:getPlaybackRegions()) {
            const auto regionSamples=region->getSampleRange(rate,juce::ARAPlaybackRegion::IncludeHeadAndTail::no);
            const auto intersect=regionSamples.getIntersectionWith(juce::Range<juce::int64>::withStartAndLength(start,count));
            if(intersect.isEmpty()) continue;
            auto* source=region->getAudioModification()->getAudioSource<AnalyzedSource>();
            const auto map=mapping(*region);
            const auto sourceRate=source->getSampleRate();
            const double first=map.toSource(double(intersect.getStart())/rate)*sourceRate;
            const auto sourceStart=static_cast<juce::int64>(std::floor(first));
            const int n=static_cast<int>(intersect.getLength());
            const double ratio=sourceRate/rate;
            const int needed=static_cast<int>(std::ceil(first-double(sourceStart)+(n-1)*ratio))+2;
            if(needed>samples.getNumSamples() || !source->playback.read(samples,needed,sourceStart,realtime==juce::AudioProcessor::Realtime::no ? 5000 : 0)) return true;
            const int offset=static_cast<int>(intersect.getStart()-start);
            for(int c=0;c<output.getNumChannels();++c) {
                auto* dst=rendered.getWritePointer(c,offset);
                for(int i=0;i<n;++i) {
                    double p=first-double(sourceStart)+i*ratio;
                    // Remove floating-point roundoff on unresampled material.
                    if(std::abs(p-std::round(p))<1e-7) p=std::round(p);
                    float value=pg::sourceSample(samples.getReadPointer(std::min(c,source->getChannelCount()-1)),needed,p);
                    if(output.getNumChannels()==1 && source->getChannelCount()==2)
                        value=.5f*(value+pg::sourceSample(samples.getReadPointer(1),needed,p));
                    dst[i]+=value;
                }
            }
        }
        for(int c=0;c<output.getNumChannels();++c) output.copyFrom(c,0,rendered,c,0,count);
        return true;
    }
    using juce::ARAPlaybackRenderer::processBlock;
    static pg::RegionMapping mapping(const juce::ARAPlaybackRegion& r) {
        return {r.getStartInPlaybackTime(),r.getStartInAudioModificationTime(),
            r.getDurationInAudioModificationTime(),r.getDurationInPlaybackTime()};
    }
private:
    ProcessingLockInterface& lock;
    juce::AudioBuffer<float> samples,rendered;
    double rate=0;
    int maxBlock=0;
};
