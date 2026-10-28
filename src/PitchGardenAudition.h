#pragma once
#include "PitchGardenPlayback.h"
#include "PitchGardenRegion.h"
#include "EditorCoordinates.h"
#include <mutex>

struct NoteAuditionRequest {
    std::uint64_t region=0,sourceRevision=0;
    pg::RegionMapping mapping;
    pg::SourceInterval interval;
};
enum class NoteAuditionStatus { idle,queued,buffering,playing,finished,invalid,hostPlaying };
// Optional ARA editor-renderer role only. This adds a stopped, realtime preview
// to its input; it never requests host play/seek/cycle changes or replaces track
// audio. Requests contain values, not host-owned pointers. Rendering resolves an
// assigned region under model/assignment gates, then uses the existing cache.
class PitchGardenAudition final : public juce::ARAEditorRenderer {
public:
    PitchGardenAudition(ARA::PlugIn::DocumentController* dc,ProcessingLockInterface& modelLock)
        : ARAEditorRenderer(dc),lock(modelLock) {}
    bool submit(const NoteAuditionRequest& next) {
        const auto range=pg::auditionInterval(next.mapping,next.interval.start,next.interval.end);
        if(!range || !juce::exactlyEqual(range->start,next.interval.start)
            || !juce::exactlyEqual(range->end,next.interval.end)) return false;
        const auto guard=assignments.tryRead();
        if(!guard.isLocked() || findRegion(next.region)==nullptr) return false;
        const std::lock_guard<std::mutex> writer(requestWriters);
        const pg::RenderGate::WriteToken write(requestGate);
        if(!prepared.load()) return false;
        request=next; ++serial;
        status.store(NoteAuditionStatus::queued); enabled.store(true); return true;
    }
    // Message/model/lifecycle thread only: drain an in-flight preview block so
    // neither samples nor a stale playing status can be published after return.
    void stop() {
        const std::lock_guard<std::mutex> writer(requestWriters);
        const pg::RenderGate::WriteToken write(requestGate);
        enabled.store(false); status.store(NoteAuditionStatus::idle);
    }
    bool isPrepared() const { return prepared.load(); }
    NoteAuditionStatus getStatus() const { return status.load(); }
    std::uint64_t callbackCount() const { return callbacks.load(); }
    void prepareToPlay(double sampleRate,int maximum,int,juce::AudioProcessor::ProcessingPrecision,
                       AlwaysNonRealtime) override {
        const std::lock_guard<std::mutex> writer(requestWriters);
        const pg::RenderGate::WriteToken write(requestGate);
        prepared.store(false); enabled.store(false); status.store(NoteAuditionStatus::idle);
        rate=sampleRate; maximumBlock=maximum;
        samples.setSize(2,static_cast<int>(std::ceil(maximum*384000.0/sampleRate))+4);
        lastSerial=0; cursor=0; prepared.store(true);
    }
    void releaseResources() override {
        const std::lock_guard<std::mutex> writer(requestWriters);
        const pg::RenderGate::WriteToken write(requestGate);
        prepared.store(false); enabled.store(false); status.store(NoteAuditionStatus::idle);
        samples.setSize(0,0); rate=0;
    }
    bool processBlock(juce::AudioBuffer<float>& output,juce::AudioProcessor::Realtime realtime,
                      const juce::AudioPlayHead::PositionInfo& position) noexcept override {
        if(realtime==juce::AudioProcessor::Realtime::no) return true;
        callbacks.fetch_add(1);
        if(!enabled.load()) return true;
        const auto read=requestGate.tryRead(); if(!read.isLocked() || !enabled.load()) return true;
        if(position.getIsPlaying()) { enabled.store(false); status.store(NoteAuditionStatus::hostPlaying); return true; }
        const auto model=lock.getProcessingLock(); if(!model.isLocked()) return true;
        const auto assigned=assignments.tryRead(); if(!assigned.isLocked()) return true;
        auto* region=findRegion(request.region);
        if(region==nullptr || !sameMapping(PitchGardenPlayback::mapping(*region),request.mapping)
            || region->isTimestretchEnabled() || region->isTimeStretchReflectingTempo()) { invalidate(); return true; }
        auto* source=region->getAudioModification()->getAudioSource<AnalyzedSource>();
        if(source->revision.load()!=request.sourceRevision) { invalidate(); return true; }
        if(rate<=0 || output.getNumSamples()>maximumBlock) return true;
        if(lastSerial!=serial) { lastSerial=serial; cursor=0; }
        const auto sourceRate=source->getSampleRate();
        const auto remaining=static_cast<juce::int64>(std::ceil((request.interval.end-request.interval.start)*rate))-cursor;
        const int count=static_cast<int>(std::min<juce::int64>(output.getNumSamples(),remaining));
        if(count<=0) { enabled.store(false); status.store(NoteAuditionStatus::finished); return true; }
        const double first=(request.interval.start+double(cursor)/rate)*sourceRate;
        const auto start=static_cast<juce::int64>(std::floor(first));
        const auto ratio=sourceRate/rate;
        const int needed=static_cast<int>(std::ceil(first-double(start)+(count-1)*ratio))+2;
        if(needed>samples.getNumSamples() || !source->playback.read(samples,needed,start,0)) {
            status.store(NoteAuditionStatus::buffering); return true;
        }
        for(int c=0;c<output.getNumChannels();++c) for(int i=0;i<count;++i) {
            double p=first-double(start)+i*ratio;
            if(std::abs(p-std::round(p))<1e-7) p=std::round(p);
            float value=pg::sourceSample(samples.getReadPointer(std::min(c,source->getChannelCount()-1)),needed,p);
            if(output.getNumChannels()==1 && source->getChannelCount()==2)
                value=.5f*(value+pg::sourceSample(samples.getReadPointer(1),needed,p));
            const double elapsed=double(cursor+i)/rate;
            const double left=request.interval.end-request.interval.start-elapsed;
            const auto gain=static_cast<float>(std::clamp(std::min(elapsed,left)/.005,0.,1.));
            output.addSample(c,i,value*gain); // fades are confined to preview, never track playback
        }
        cursor+=count; status.store(NoteAuditionStatus::playing);
        if(cursor>=static_cast<juce::int64>(std::ceil((request.interval.end-request.interval.start)*rate))) {
            enabled.store(false); status.store(NoteAuditionStatus::finished);
        }
        return true;
    }
    using juce::ARAEditorRenderer::processBlock;
private:
    static bool sameMapping(const pg::RegionMapping& a,const pg::RegionMapping& b) {
        return juce::exactlyEqual(a.timelineStart,b.timelineStart) && juce::exactlyEqual(a.sourceStart,b.sourceStart)
            && juce::exactlyEqual(a.sourceDuration,b.sourceDuration) && juce::exactlyEqual(a.playbackDuration,b.playbackDuration);
    }
    PitchGardenRegion* findRegion(std::uint64_t identity) const {
        for(auto* region:getPlaybackRegions<PitchGardenRegion>()) if(region->identity==identity) return region;
        for(auto* sequence:getRegionSequences())
            for(auto* region:sequence->getPlaybackRegions<PitchGardenRegion>()) if(region->identity==identity) return region;
        return nullptr;
    }
    void invalidate() { enabled.store(false); status.store(NoteAuditionStatus::invalid); }
    void willAddPlaybackRegion(ARA::PlugIn::PlaybackRegion*) noexcept override { assignments.beginWrite(); }
    void didAddPlaybackRegion(ARA::PlugIn::PlaybackRegion*) noexcept override { assignments.endWrite(); }
    void willRemovePlaybackRegion(ARA::PlugIn::PlaybackRegion*) noexcept override {
        assignments.beginWrite(); invalidate();
    }
    void didRemovePlaybackRegion(ARA::PlugIn::PlaybackRegion*) noexcept override { assignments.endWrite(); }
    void willAddRegionSequence(ARA::PlugIn::RegionSequence*) noexcept override { assignments.beginWrite(); }
    void didAddRegionSequence(ARA::PlugIn::RegionSequence*) noexcept override { assignments.endWrite(); }
    void willRemoveRegionSequence(ARA::PlugIn::RegionSequence*) noexcept override {
        assignments.beginWrite(); invalidate();
    }
    void didRemoveRegionSequence(ARA::PlugIn::RegionSequence*) noexcept override { assignments.endWrite(); }
    ProcessingLockInterface& lock;
    pg::RenderGate assignments,requestGate;
    // GUI and host lifecycle callers can differ. Serialize non-RT writers only;
    // processBlock never touches this mutex and only tries the atomic read gate.
    std::mutex requestWriters;
    NoteAuditionRequest request;
    std::uint64_t serial=0,lastSerial=0;
    juce::int64 cursor=0;
    std::atomic<std::uint64_t> callbacks{0};
    std::atomic<bool> enabled{false},prepared{false};
    std::atomic<NoteAuditionStatus> status{NoteAuditionStatus::idle};
    juce::AudioBuffer<float> samples;
    double rate=0;
    int maximumBlock=0;
};
