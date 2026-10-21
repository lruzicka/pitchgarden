#include "TransportState.h"
#include "PitchAnalysis.h"
#include <iostream>
#include <limits>
#include <thread>
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; return 1; } } while(false)
int main() {
    auto state=pg::mapTransport(true,true,12.5,123,48000);
    CHECK(state.available && state.playing && state.looping && state.projectSeconds==12.5);
    state=pg::mapTransport(false,false,{},96000,48000); CHECK(state.available && state.projectSeconds==2);
    CHECK(!pg::mapTransport(false,false,{},123,0).available);
    CHECK(!pg::mapTransport(false,false,std::numeric_limits<double>::quiet_NaN(),{},48000).available);
    pg::TransportState published;
    for(double time : {12.,13.,5.,6.,12.}) { published.publish({time,true,true,true}); CHECK(published.read().projectSeconds==time); }
    published.publish({3,true,false,false}); CHECK(!published.read().playing && published.read().projectSeconds==3);
    CHECK(pg::followStart(0,10,{9,true,true,false},true)==7.5);
    CHECK(pg::followStart(10,10,{2,true,true,true},true)==.5);
    CHECK(pg::followStart(0,10,{99,true,true,false},false)==0);
    CHECK(pg::followStart(0,10,{99,true,false,false},true)==0);
    pg::RegionMapping region{12,3,5,5};
    CHECK(region.toSource(13)==4); CHECK(region.toTimeline(4)==13);
    region.timelineStart=30; CHECK(region.toTimeline(4)==31); CHECK(region.toSource(31)==4);
    region.sourceStart=4; region.sourceDuration=region.playbackDuration=2;
    CHECK(region.toTimeline(4)==30); CHECK(region.toSource(32)==6);
    CHECK(pg::RegionMapping({-5,3,2,2}).toTimeline(4)==-4);
    CHECK(!pg::RegionMapping({12,3,2,3}).supported());
    published.publish({0,true,false,false});
    std::thread writer([&] { for(int i=0;i<10000;++i) published.publish({double(i),true,(i%2)!=0,(i%3)!=0}); });
    bool coherent=true;
    for(int i=0;i<10000;++i) { auto s=published.read(); if(s.available) {
        const auto n=int(s.projectSeconds); if(s.playing!=((n%2)!=0) || s.looping!=((n%3)!=0)) coherent=false;
    } }
    writer.join(); CHECK(coherent);
    std::cout<<"transport mapping, seek/loop/stop, follow, coordinates and publication passed\n";
}
