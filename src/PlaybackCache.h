#pragma once
#include "JuceHeader.h"
// All configuration is on the ARA model thread. Reads are bounded, nonblocking
// cache reads. The time-slice thread alone calls the host's audio reader.
class PlaybackReadThread final : public juce::TimeSliceThread {
public:
    PlaybackReadThread() : TimeSliceThread("PitchGarden playback cache") { startThread(juce::Thread::Priority::high); }
    ~PlaybackReadThread() override { stopThread(-1); }
};
class PlaybackCache {
public:
    void reset() { const juce::ScopedWriteLock guard(lifetime); reader.reset(); }
    void configure(juce::ARAAudioSource* source) {
        reset();
        const juce::ScopedWriteLock guard(lifetime);
        reader=std::make_unique<juce::BufferingAudioReader>(new juce::ARAAudioSourceReader(source),*thread,
            juce::jmax(65536,static_cast<int>(source->getSampleRate()*2)));
        reader->setReadTimeout(0);
    }
    bool read(juce::AudioBuffer<float>& target,int count,juce::int64 start,int timeout) {
        const juce::ScopedTryReadLock guard(lifetime);
        const juce::SpinLock::ScopedTryLockType singleReader(reading);
        if(!guard.isLocked() || !singleReader.isLocked() || !reader) { unavailable.store(true); return false; }
        reader->setReadTimeout(timeout);
        const bool success=reader->read(&target,0,count,start,true,true);
        unavailable.store(!success); return success;
    }
    std::atomic<bool> unavailable{false};
private:
    juce::SharedResourcePointer<PlaybackReadThread> thread;
    juce::ReadWriteLock lifetime;
    juce::SpinLock reading;
    std::unique_ptr<juce::BufferingAudioReader> reader;
};
