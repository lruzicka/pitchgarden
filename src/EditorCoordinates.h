#pragma once
#include "PitchAnalysis.h"
#include <algorithm>
#include <cmath>
#include <optional>
namespace pg {
struct SourceInterval { double start=0,end=0; };
inline std::optional<SourceInterval> auditionInterval(const RegionMapping& mapping,double noteStart,double noteEnd) {
    if(!mapping.supported() || !std::isfinite(noteStart) || !std::isfinite(noteEnd)) return {};
    const auto start=std::max(noteStart,mapping.sourceStart);
    const auto end=std::min(noteEnd,mapping.sourceStart+mapping.sourceDuration);
    if(end<=start) return {};
    return SourceInterval{start,end};
}
}
