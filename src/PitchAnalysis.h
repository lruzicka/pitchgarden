#pragma once
#include "PitchDetector.h"
#include <atomic>
#include <cstdint>
#include <functional>

namespace pg {
double frequencyToMidi(double hz);
double frameTime(std::int64_t startSample, std::size_t window, double sampleRate);
struct RegionMapping {
    double timelineStart=0, sourceStart=0, sourceDuration=0, playbackDuration=0;
    bool supported() const;
    double toTimeline(double sourceTime) const { return timelineStart + sourceTime - sourceStart; }
    double toSource(double timelineTime) const { return sourceStart + timelineTime - timelineStart; }
};
struct AnalysisSettings {
    std::size_t window=2048, hop=256;
    double minHz=65, maxHz=1200, threshold=.15;
    void validate(double sampleRate) const;
    // At 96 kHz, 2048 samples cannot cover a full 65 Hz YIN lag plus comparison
    // interval. Enlarge the sample window (hop remains 256 samples).
    std::size_t windowFor(double sampleRate) const;
};
using MonoReader = std::function<bool(std::int64_t,int,float*)>;
std::vector<PitchFrame> analyzeSource(double sampleRate, std::int64_t sampleCount,
    const MonoReader&, const AnalysisSettings& = {}, const std::atomic<bool>* cancelled=nullptr,
    const std::function<void(float)>& progress={});
}
