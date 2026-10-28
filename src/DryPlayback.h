#pragma once
#include <algorithm>
#include <cmath>
namespace pg {
// Linear sample-rate conversion only. At integral positions this returns the
// exact original sample. No gain, pitch correction, fades, or note edits.
inline float sourceSample(const float* data,int count,double position) noexcept {
    if(!data || count<=0 || !std::isfinite(position) || position<0 || position>count-1) return 0;
    const auto i=static_cast<int>(position);
    const auto fraction=static_cast<float>(position-i);
    if(fraction<=0 || i+1>=count) return data[i];
    return data[i]+fraction*(data[i+1]-data[i]);
}
}
