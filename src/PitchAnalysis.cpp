#include "PitchAnalysis.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace pg {
double frequencyToMidi(double hz) {
    return std::isfinite(hz) && hz>0 ? 69+12*std::log2(hz/440) : std::numeric_limits<double>::quiet_NaN();
}
double frameTime(std::int64_t start, std::size_t window, double sr) {
    if (!std::isfinite(sr) || sr<=0) throw std::invalid_argument("Invalid sample rate");
    return (double(start)+double(window/2))/sr;
}
bool RegionMapping::supported() const {
    return std::isfinite(timelineStart) && std::isfinite(sourceStart)
        && std::isfinite(sourceDuration) && std::isfinite(playbackDuration)
        && sourceStart>=0 && sourceDuration>0 && playbackDuration>0
        && std::abs(sourceDuration-playbackDuration)<1e-7;
}
void AnalysisSettings::validate(double sr) const {
    if (!std::isfinite(sr) || sr<8000 || sr>384000 || window<32 || window>1048576
        || hop==0 || hop>window || !std::isfinite(minHz) || !std::isfinite(maxHz)
        || minHz<=0 || maxHz<=minHz || maxHz>=sr/2 || !std::isfinite(threshold)
        || threshold<=0 || threshold>=1 || 2*sr/minHz>1048574)
        throw std::invalid_argument("Invalid pitch analysis settings");
}
std::size_t AnalysisSettings::windowFor(double sr) const {
    validate(sr);
    return std::max(window, static_cast<std::size_t>(std::ceil(2*sr/minHz))+2);
}
std::vector<PitchFrame> analyzeSource(double sr, std::int64_t count, const MonoReader& read,
    const AnalysisSettings& cfg, const std::atomic<bool>* cancel, const std::function<void(float)>& progress) {
    const auto window=cfg.windowFor(sr);
    if (count<0 || !read) throw std::invalid_argument("Invalid audio source");
    std::vector<PitchFrame> result;
    std::vector<float> samples(window);
    for (std::int64_t pos=0; pos<=count-static_cast<std::int64_t>(window); pos+=static_cast<std::int64_t>(cfg.hop)) {
        if (cancel && cancel->load()) return {};
        // Bounded sample acquisition; never buffer an entire recording.
        if (!read(pos,static_cast<int>(window),samples.data())) throw std::runtime_error("ARA sample read failed or unavailable");
        auto pitch=detectPitch(samples.data(),window,sr,cfg.minHz,cfg.maxHz,cfg.threshold);
        if (!std::isfinite(pitch.hz) || pitch.hz<=0 || !std::isfinite(pitch.confidence)) pitch={};
        if (pitch.voiced) pitch.midi=frequencyToMidi(pitch.hz);
        result.push_back({frameTime(pos,window,sr),pitch});
        if (progress) progress(float(double(pos+static_cast<std::int64_t>(window))/double(count)));
    }
    return result;
}
}
