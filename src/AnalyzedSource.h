#pragma once
#include "ARAAnalysisBridge.h"
#include "AnalysisModel.h"
#include "PlaybackCache.h"

class AnalyzedSource final : public juce::ARAAudioSource,
                             private juce::ARAAudioSource::Listener {
public:
    AnalyzedSource(juce::ARADocument* document, ARA::ARAAudioSourceHostRef hostRef, pg::AnalysisSettings cfg={})
        : ARAAudioSource(document,hostRef),settings(cfg) { addListener(this); }
    ~AnalyzedSource() override { stop(); removeListener(this); }
    void ensureAnalysis() { if(restartNeeded) { restartNeeded=false; start(); } }
    PlaybackCache playback;
    std::atomic<std::uint64_t> revision{0};
    pg::AnalysisSnapshot snapshot() const { return analysis.snapshot(); }
private:
    void stop() { revision.fetch_add(1); analysis.cancel(); bridge.reset(); playback.reset(); }
    void start() {
        restartNeeded=false;
        stop();
        if(!isSampleAccessEnabled()) return;
        try {
            bridge=std::make_unique<ARAAnalysisBridge>(this,settings);
            playback.configure(this);
            // Only the JUCE reader crosses the thread boundary, never the source.
            auto* reader=bridge.get();
            analysis.start(reader->sampleRate,reader->sampleCount,
                [reader](std::int64_t p,int n,float* dst) { return reader->readMono(p,n,dst); },settings);
        } catch(const std::exception& e) { analysis.fail(e.what()); bridge.reset(); }
    }
    void willUpdateAudioSourceProperties(juce::ARAAudioSource*, PropertiesPtr) override { stop(); }
    void didUpdateAudioSourceProperties(juce::ARAAudioSource*) override { restartNeeded=true; }
    void doUpdateAudioSourceContent(juce::ARAAudioSource*, juce::ARAContentUpdateScopes scopes) override {
        if(scopes.affectSamples()) { stop(); restartNeeded=true; }
    }
    void willEnableAudioSourceSamplesAccess(juce::ARAAudioSource*,bool enable) override { if(!enable) stop(); }
    void didEnableAudioSourceSamplesAccess(juce::ARAAudioSource*,bool enable) override { if(enable) start(); }
    void willDeactivateAudioSourceForUndoHistory(juce::ARAAudioSource*,bool deactivate) override { if(deactivate) stop(); }
    void didDeactivateAudioSourceForUndoHistory(juce::ARAAudioSource*,bool deactivate) override { if(!deactivate) start(); }
    void willDestroyAudioSource(juce::ARAAudioSource*) override { stop(); }
    const pg::AnalysisSettings settings;
    bool restartNeeded=false;
    std::unique_ptr<ARAAnalysisBridge> bridge;
    pg::AnalysisModel analysis;
};
