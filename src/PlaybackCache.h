#pragma once
#include "JuceHeader.h"
#include <array>
#include "RenderGate.h"

class PlaybackReadThread final : public juce::TimeSliceThread {
public:
    PlaybackReadThread() : TimeSliceThread("PitchGarden playback cache") { startThread(juce::Thread::Priority::high); }
    ~PlaybackReadThread() override { stopThread(-1); }
};
// Four independent read-ahead windows support regions at different source offsets.
// The worker reads into staging without holding a lock. Publication swaps buffers
// under a short try-lock. Realtime reads only try-lock and copy already cached data;
// they never enter JUCE's blocking BufferingAudioReader or the host sample reader.
class PlaybackCache final : private juce::TimeSliceClient {
public:
    ~PlaybackCache() override { reset(); }
    void reset() {
        const pg::RenderGate::WriteToken guard(lifetime);
        thread->removeTimeSliceClient(this); // model thread, after realtime readers leave
        reader.reset();
        for(auto& slot:slots) slot.start=-1;
    }
    void configure(juce::ARAAudioSource* source) {
        reset();
        const pg::RenderGate::WriteToken guard(lifetime);
        reader=std::make_unique<juce::ARAAudioSourceReader>(source);
        channels=source->getChannelCount();
        capacity=juce::jmax(65536,static_cast<int>(source->getSampleRate()*2));
        staging.setSize(channels,capacity);
        for(auto& slot:slots) { slot.audio.setSize(channels,capacity); slot.start=-1; }
        requested.store(0); nextSlot=0; unavailable.store(false);
        thread->addTimeSliceClient(this);
    }
    bool read(juce::AudioBuffer<float>& target,int count,juce::int64 start,int timeout) {
        const auto guard=lifetime.tryRead();
        if(!guard.isLocked() || !reader || count>capacity) { unavailable.store(true); return false; }
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+timeout;
        do {
            {
                const juce::SpinLock::ScopedTryLockType cacheGuard(dataLock);
                if(cacheGuard.isLocked()) for(const auto& slot:slots) {
                    if(slot.start>=0 && start>=slot.start && start+count<=slot.start+capacity) {
                        for(int c=0;c<target.getNumChannels();++c)
                            target.copyFrom(c,0,slot.audio,std::min(c,channels-1),static_cast<int>(start-slot.start),count);
                        requested.store(start+capacity/2); // prefetch before this window ends
                        unavailable.store(false); return true;
                    }
                }
            }
            requested.store(std::max<juce::int64>(0,start));
            if(timeout<=0) break;
            juce::Thread::sleep(1); // offline rendering only
        } while(juce::Time::getMillisecondCounterHiRes()<deadline);
        unavailable.store(true); return false;
    }
    std::atomic<bool> unavailable{false};
private:
    struct Slot { juce::AudioBuffer<float> audio; juce::int64 start=-1; };
    int useTimeSlice() override {
        const auto position=requested.load();
        {
            const juce::SpinLock::ScopedTryLockType guard(dataLock);
            if(!guard.isLocked()) return 1;
            for(const auto& slot:slots)
                if(slot.start>=0 && position>=slot.start && position+capacity/4<=slot.start+capacity) return 5;
        }
        if(!reader->read(&staging,0,capacity,position,true,true)) { unavailable.store(true); return 50; }
        {
            const juce::SpinLock::ScopedTryLockType guard(dataLock);
            if(!guard.isLocked()) return 1;
            std::swap(slots[nextSlot].audio,staging);
            slots[nextSlot].start=position;
            nextSlot=(nextSlot+1)%slots.size();
        }
        return 1;
    }
    juce::SharedResourcePointer<PlaybackReadThread> thread;
    pg::RenderGate lifetime;
    juce::SpinLock dataLock;
    std::unique_ptr<juce::ARAAudioSourceReader> reader;
    std::array<Slot,4> slots;
    juce::AudioBuffer<float> staging;
    std::atomic<juce::int64> requested{0};
    std::size_t nextSlot=0;
    int channels=0,capacity=0;
};
