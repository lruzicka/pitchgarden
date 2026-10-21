#include "NoteSegmenter.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace pg {
namespace {
double median(std::vector<double> values) {
    const auto mid=values.begin()+static_cast<std::ptrdiff_t>(values.size()/2);
    std::nth_element(values.begin(),mid,values.end());
    const double upper=*mid;
    return values.size()%2 ? upper : (upper+*std::max_element(values.begin(),mid))/2;
}
bool valid(const PitchFrame& f,const SegmentationSettings& s) {
    return f.pitch.voiced && std::isfinite(f.pitch.midi) && std::isfinite(f.pitch.confidence)
        && f.pitch.confidence>=s.minimumConfidence;
}
}
std::vector<Note> segmentNotes(const std::vector<PitchFrame>& f,double period,const SegmentationSettings& s) {
    if(!std::isfinite(period) || period<=0 || !std::isfinite(s.minimumDuration) || s.minimumDuration<=0
        || !std::isfinite(s.maximumGap) || s.maximumGap<0 || !std::isfinite(s.minimumConfidence)
        || s.minimumConfidence<0 || s.minimumConfidence>1 || !std::isfinite(s.transitionSemitones)
        || s.transitionSemitones<=0 || !std::isfinite(s.transitionHold) || s.transitionHold<=0
        || !std::isfinite(s.transitionSpread) || s.transitionSpread<=0)
        throw std::invalid_argument("Invalid segmentation settings");
    std::vector<Note> notes;
    const auto none=f.size();
    std::size_t first=none,last=none,candidate=none;
    double anchor=0;
    std::vector<double> initial,transition;
    const auto finish=[&](std::size_t end,double endTime) {
        if(first==none) return;
        const auto start=std::max(0.,f[first].seconds-period/2);
        if(endTime-start+1e-9<s.minimumDuration) return;
        std::vector<double> pitches; double quality=0;
        for(auto j=first;j<end;++j) if(valid(f[j],s)) { pitches.push_back(f[j].pitch.midi); quality+=f[j].pitch.confidence; }
        if(pitches.empty()) return;
        const auto pitch=median(pitches);
        const auto midi=static_cast<int>(std::lround(pitch));
        notes.push_back({start,endTime,midi,pitch,100*(pitch-midi),quality/double(pitches.size()),first,end});
    };
    for(std::size_t i=0;i<f.size();++i) {
        if(!std::isfinite(f[i].seconds) || f[i].seconds<0 || (i>0 && f[i].seconds<=f[i-1].seconds))
            throw std::invalid_argument("Pitch frames must have ordered finite source times");
        if(first!=none && f[i].seconds-f[last].seconds>s.maximumGap+period+1e-9) {
            finish(last+1,f[last].seconds+period/2); first=none; candidate=none; transition.clear();
        }
        if(!valid(f[i],s)) continue;
        if(first==none) {
            first=last=i; anchor=f[i].pitch.midi; initial={anchor}; candidate=none; continue;
        }
        last=i;
        const double pitch=f[i].pitch.midi;
        if(std::abs(pitch-anchor)<s.transitionSemitones) {
            candidate=none; transition.clear();
            if(initial.size()<11) { initial.push_back(pitch); anchor=median(initial); }
            continue;
        }
        if(candidate==none || (pitch-anchor)*(f[candidate].pitch.midi-anchor)<0) { candidate=i; transition.clear(); }
        transition.push_back(pitch);
        const auto limits=std::minmax_element(transition.begin(),transition.end());
        if(*limits.second-*limits.first>s.transitionSpread) { candidate=i; transition={pitch}; }
        if(f[i].seconds-f[candidate].seconds+period>=s.transitionHold) {
            finish(candidate,std::max(0.,f[candidate].seconds-period/2));
            first=candidate; anchor=median(transition); initial=transition;
            candidate=none; transition.clear();
        }
    }
    if(first!=none) finish(last+1,f[last].seconds+period/2);
    return notes;
}
}
