#include "PitchQuality.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace pg {
std::vector<PitchQuality> classifyPitchQuality(const std::vector<PitchFrame>& f,double period,const PitchQualitySettings& s) {
    if(!std::isfinite(period) || period<=0 || !std::isfinite(s.minimumConfidence)
        || s.minimumConfidence<0 || s.minimumConfidence>1 || !std::isfinite(s.maximumExcursion)
        || s.maximumExcursion<=0 || !std::isfinite(s.octaveTolerance) || s.octaveTolerance<=0
        || !std::isfinite(s.returnTolerance) || s.returnTolerance<=0)
        throw std::invalid_argument("Invalid pitch quality settings");
    std::vector<PitchQuality> quality(f.size(),PitchQuality::unvoiced);
    for(std::size_t i=0;i<f.size();++i) {
        if(!std::isfinite(f[i].seconds) || (i>0 && f[i].seconds<=f[i-1].seconds))
            throw std::invalid_argument("Pitch quality requires ordered finite timestamps");
        const auto& p=f[i].pitch;
        if(p.voiced && std::isfinite(p.midi) && std::isfinite(p.confidence) && p.confidence>=0 && p.confidence<=1)
            quality[i]=p.confidence>=s.minimumConfidence ? PitchQuality::reliable : PitchQuality::lowConfidence;
    }
    const auto adjacent=[&](std::size_t a,std::size_t b) { return f[b].seconds-f[a].seconds<=period*1.5; };
    for(std::size_t i=3;i+3<f.size();++i) {
        if(quality[i]!=PitchQuality::reliable) continue;
        double prior[3]={f[i-3].pitch.midi,f[i-2].pitch.midi,f[i-1].pitch.midi};
        std::sort(std::begin(prior),std::end(prior)); const auto anchor=prior[1];
        bool stable=true;
        for(auto k=i-3;k<i;++k)
            if(quality[k]!=PitchQuality::reliable || std::abs(f[k].pitch.midi-anchor)>s.returnTolerance || !adjacent(k,k+1)) stable=false;
        if(!stable) continue;
        const auto jump=f[i].pitch.midi-anchor;
        if(std::abs(jump)<11 || std::abs(jump)>25) continue;
        const double octave=12*std::round(jump/12);
        if(std::abs(jump-octave)>s.octaveTolerance) continue;
        for(auto j=i;j+2<f.size() && f[j].seconds-f[i].seconds<=s.maximumExcursion+1e-9;++j) {
            if(quality[j]!=PitchQuality::reliable || (j>i && !adjacent(j-1,j))) break;
            if(std::abs(f[j].pitch.midi-anchor)<=s.returnTolerance) {
                bool returned=true;
                for(auto k=j;k<j+3;++k)
                    if(quality[k]!=PitchQuality::reliable || std::abs(f[k].pitch.midi-anchor)>s.returnTolerance
                        || (k>j && !adjacent(k-1,k))) returned=false;
                if(returned) { std::fill(quality.begin()+static_cast<std::ptrdiff_t>(i),quality.begin()+static_cast<std::ptrdiff_t>(j),PitchQuality::octaveExcursion); i=j-1; }
                break;
            }
            if(std::abs(f[j].pitch.midi-anchor-octave)>s.octaveTolerance) break;
        }
    }
    return quality;
}
}
