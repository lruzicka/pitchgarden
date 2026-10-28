#pragma once
#include "PitchAnalysis.h"
#include <algorithm>
#include <cmath>
#include <optional>
namespace pg {
// The roll axis is in project seconds. Source offsets have already been
// applied when drawing/hit-testing notes; never apply them again to a click.
inline std::optional<double> projectTimeAtPixel(double pixel,double plotLeft,double plotWidth,
                                               double viewStart,double viewDuration) {
    if(!std::isfinite(pixel) || !std::isfinite(plotLeft) || !std::isfinite(plotWidth)
        || plotWidth<=0 || !std::isfinite(viewStart) || !std::isfinite(viewDuration) || viewDuration<=0) return {};
    const auto time=viewStart+std::clamp((pixel-plotLeft)/plotWidth,0.,1.)*viewDuration;
    return std::isfinite(time) ? std::optional<double>(time) : std::nullopt;
}
struct SourceInterval { double start=0,end=0; };
inline std::optional<SourceInterval> auditionInterval(const RegionMapping& mapping,double noteStart,double noteEnd) {
    if(!mapping.supported() || !std::isfinite(noteStart) || !std::isfinite(noteEnd)) return {};
    const auto start=std::max(noteStart,mapping.sourceStart);
    const auto end=std::min(noteEnd,mapping.sourceStart+mapping.sourceDuration);
    if(end<=start) return {};
    return SourceInterval{start,end};
}
}
