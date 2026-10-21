#include "AnalysisModel.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <iostream>
#include <thread>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; return 1; } } while(false)
bool wait(pg::AnalysisModel& m) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(m.snapshot().state==pg::AnalysisState::analyzing && std::chrono::steady_clock::now()<end)
        std::this_thread::yield();
    return m.snapshot().state!=pg::AnalysisState::analyzing;
}
int main() {
    pg::AnalysisModel m;
    CHECK(m.snapshot().state==pg::AnalysisState::waiting);
    m.start(48000,4096,[](auto pos,int n,float* dst) {
        for(int i=0;i<n;++i) dst[i]=float(.7*std::sin(2*3.141592653589793*440*(pos+i)/48000)); return true;
    });
    CHECK(wait(m)); auto old=m.snapshot(); CHECK(old.state==pg::AnalysisState::ready);
    CHECK(old.result && !old.result->frames.empty());
    m.cancel(); CHECK(m.snapshot().state==pg::AnalysisState::waiting); CHECK(!m.snapshot().result);
    CHECK(old.result->frames[0].pitch.voiced); // detached immutable snapshot stays valid
    m.start(48000,4096,[](auto,int,float*) { return false; });
    CHECK(wait(m)); CHECK(m.snapshot().state==pg::AnalysisState::failed); CHECK(!m.snapshot().error.empty());
    std::promise<void> reading; auto began=reading.get_future();
    std::promise<void> release; auto resume=release.get_future().share();
    std::atomic<bool> signalled{false};
    m.start(48000,480000,[&](auto,int n,float* dst) { if(!signalled.exchange(true)) reading.set_value(); resume.wait(); std::fill_n(dst,n,0.f); return true; });
    began.wait(); release.set_value(); m.cancel();
    CHECK(m.snapshot().state==pg::AnalysisState::waiting); CHECK(!m.snapshot().result);
    m.start(48000,4096,[](auto,int n,float* dst) { std::fill_n(dst,n,0.f); return true; });
    CHECK(wait(m)); CHECK(m.snapshot().state==pg::AnalysisState::ready);
    { pg::AnalysisModel shuttingDown; shuttingDown.start(48000,480000,[](auto,int n,float* dst) { std::fill_n(dst,n,0.f); return true; }); }
    std::cout << "publication, invalidation, failure, cancellation, restart and shutdown passed\n";
}
