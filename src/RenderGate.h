#pragma once
#include <atomic>
#include <thread>
namespace pg {
// A single model writer excludes render readers. Render entry/exit is bounded
// atomic work; only the model thread waits, before changing the ARA graph.
class RenderGate {
public:
    class ReadToken {
    public:
        explicit ReadToken(RenderGate& g) : gate(g) {
            if(gate.writing.load()) return;
            gate.readers.fetch_add(1);
            acquired=!gate.writing.load();
            if(!acquired) gate.readers.fetch_sub(1);
        }
        ~ReadToken() { if(acquired) gate.readers.fetch_sub(1); }
        ReadToken(const ReadToken&)=delete;
        ReadToken& operator=(const ReadToken&)=delete;
        bool isLocked() const { return acquired; }
    private:
        RenderGate& gate;
        bool acquired=false;
    };
    class WriteToken {
    public:
        explicit WriteToken(RenderGate& g) : gate(g) { gate.beginWrite(); }
        ~WriteToken() { gate.endWrite(); }
        WriteToken(const WriteToken&)=delete;
        WriteToken& operator=(const WriteToken&)=delete;
    private:
        RenderGate& gate;
    };
    ReadToken tryRead() { return ReadToken(*this); }
    void beginWrite() { writing.store(true); while(readers.load()>0) std::this_thread::yield(); }
    void endWrite() { writing.store(false); }
private:
    static_assert(std::atomic<int>::is_always_lock_free && std::atomic<bool>::is_always_lock_free,
        "Realtime gates require lock-free atomics");
    std::atomic<bool> writing{false};
    std::atomic<int> readers{0};
};
}
