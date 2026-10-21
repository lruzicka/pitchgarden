#include "DryPlayback.h"
#include <cmath>
#include <iostream>
int main() {
    float samples[513]; for(int i=0;i<513;++i) samples[i]=float(std::sin(i*.17));
    for(int i=0;i<513;++i) if(pg::sourceSample(samples,513,i)!=samples[i]) return 1;
    for(int i=0;i<512;++i) if(std::abs(pg::sourceSample(samples,513,i+.5)-.5f*(samples[i]+samples[i+1]))>1e-6) return 2;
    if(pg::sourceSample(samples,513,-1)!=0 || pg::sourceSample(nullptr,0,0)!=0) return 3;
    std::cout << "unity samples and fractional sample-rate conversion passed\n";
}
