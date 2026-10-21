#include "PitchQuality.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; return 1; } } while(false)
std::vector<pg::PitchFrame> makeFrames(int count,double midi=69) {
    std::vector<pg::PitchFrame> result;
    for(int i=0;i<count;++i) result.push_back({.01*i,{440,midi,.99,true}});
    return result;
}
int countOctaves(const std::vector<pg::PitchQuality>& q) {
    return int(std::count(q.begin(),q.end(),pg::PitchQuality::octaveExcursion));
}
int main() {
    auto f=makeFrames(120); for(int i=50;i<54;++i) f[i].pitch.midi=81;
    auto q=pg::classifyPitchQuality(f,.01); CHECK(countOctaves(q)==4); CHECK(q[49]==pg::PitchQuality::reliable && q[54]==pg::PitchQuality::reliable);
    CHECK(f[50].pitch.midi==81 && f[50].pitch.confidence==.99);
    f=makeFrames(120); for(int i=50;i<54;++i) f[i].pitch.midi=57;
    CHECK(countOctaves(pg::classifyPitchQuality(f,.01))==4);
    f=makeFrames(120); for(int i=50;i<120;++i) f[i].pitch.midi=81;
    CHECK(countOctaves(pg::classifyPitchQuality(f,.01))==0); // genuine sustained octave preserved
    f=makeFrames(120); for(int i=0;i<120;++i) f[i].pitch.midi+=.65*std::sin(2*3.141592653589793*5*i*.01);
    CHECK(countOctaves(pg::classifyPitchQuality(f,.01))==0);
    f=makeFrames(120); for(int i=40;i<80;++i) f[i].pitch.midi+=12.*(i-40)/40;
    for(int i=80;i<120;++i) f[i].pitch.midi=81;
    CHECK(countOctaves(pg::classifyPitchQuality(f,.01))==0); // continuous portamento remains continuous
    f=makeFrames(120); f[50].pitch.confidence=.2; f[51].pitch={};
    q=pg::classifyPitchQuality(f,.01); CHECK(q[50]==pg::PitchQuality::lowConfidence && q[51]==pg::PitchQuality::unvoiced);
    f=makeFrames(120); for(int i=50;i<54;++i) f[i].pitch.midi=81; f[54].pitch={};
    CHECK(countOctaves(pg::classifyPitchQuality(f,.01))==0); // do not infer across silence
    f=makeFrames(120); f[0].pitch.midi=81; f[119].pitch.midi=81;
    CHECK(countOctaves(pg::classifyPitchQuality(f,.01))==0); // insufficient context at edges
    // Reproduce YIN's harmonic ambiguity: high confidence need not identify the fundamental.
    std::vector<float> signal(2048);
    for(int i=0;i<2048;++i) { const double phase=2*3.141592653589793*110*i/48000; signal[i]=float(.01*std::sin(phase)+.8*std::sin(2*phase)); }
    const auto pitch=pg::detectPitch(signal.data(),signal.size(),48000,65,1200,.15);
    CHECK(pitch.voiced && pitch.confidence>.99 && std::abs(pitch.hz-220)<1);
    f=makeFrames(120,45); for(int i=50;i<54;++i) f[i].pitch=pitch;
    CHECK(countOctaves(pg::classifyPitchQuality(f,.01))==4);
    std::cout<<"octave ambiguity, transient annotation, confidence, real octaves, vibrato and slides passed\n";
}
