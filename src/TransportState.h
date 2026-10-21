#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include <optional>
#include <limits>
namespace pg {
struct TransportSnapshot {
    double projectSeconds=0;
    bool available=false,playing=false,looping=false;
};
inline TransportSnapshot mapTransport(bool playing,bool looping,std::optional<double> seconds,
    std::optional<std::int64_t> samples,double sampleRate) {
    TransportSnapshot result;
    if(seconds && std::isfinite(*seconds)) { result.projectSeconds=*seconds; result.available=true; }
    else if(samples && std::isfinite(sampleRate) && sampleRate>0) {
        result.projectSeconds=double(*samples)/sampleRate; result.available=true;
    }
    result.playing=playing; result.looping=looping;
    return result;
}
// One audio/lifecycle writer, any non-realtime reader. Bounded publication, no
// allocation or lock. Atomic payload + sequence avoid torn transport snapshots.
class TransportState {
public:
    void publish(const TransportSnapshot& state) noexcept {
        sequence.fetch_add(1);
        seconds.store(state.projectSeconds);
        flags.store((state.available ? 1u : 0u)|(state.playing ? 2u : 0u)|(state.looping ? 4u : 0u));
        sequence.fetch_add(1);
    }
    TransportSnapshot read() const noexcept {
        for(int attempt=0;attempt<4;++attempt) {
            const auto before=sequence.load(); if(before%2!=0) continue;
            const auto time=seconds.load(); const auto bits=flags.load();
            if(before==sequence.load()) return {time,(bits&1u)!=0,(bits&2u)!=0,(bits&4u)!=0};
        }
        return {};
    }
private:
    static_assert(std::atomic<double>::is_always_lock_free && std::atomic<unsigned>::is_always_lock_free,
        "Transport publication must be lock-free");
    std::atomic<unsigned> sequence{0},flags{0};
    std::atomic<double> seconds{0};
};
inline std::optional<std::int64_t> projectSamplePosition(std::optional<std::int64_t> samples,
    std::optional<double> seconds,double sampleRate) {
    if(samples) return samples;
    if(!seconds || !std::isfinite(*seconds) || !std::isfinite(sampleRate) || sampleRate<=0) return {};
    const long double position=static_cast<long double>(*seconds)*sampleRate;
    if(position<static_cast<long double>(std::numeric_limits<std::int64_t>::min())
        || position>static_cast<long double>(std::numeric_limits<std::int64_t>::max())) return {};
    return static_cast<std::int64_t>(std::llround(position));
}
inline double followStart(double start,double span,const TransportSnapshot& state,bool enabled) {
    if(!enabled || !state.available || !state.playing || !std::isfinite(span) || span<=0) return start;
    return state.projectSeconds<start || state.projectSeconds>start+.85*span ? state.projectSeconds-.15*span : start;
}
}
