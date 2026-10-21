#include "NoteSegmenter.h"
#include <cmath>
#include <iostream>
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; return 1; } } while(false)
std::vector<pg::PitchFrame> frames(int n,double midi=69,double confidence=.99) {
    std::vector<pg::PitchFrame> f; for(int i=0;i<n;++i) f.push_back({.01*i,{440,midi,confidence,true}}); return f;
}
int main() {
    auto f=frames(100,69.17); auto notes=pg::segmentNotes(f,.01);
    CHECK(notes.size()==1); CHECK(std::abs(notes[0].cents-17)<.001);
    CHECK(notes[0].midiNote==69); CHECK(notes[0].firstFrame==0 && notes[0].endFrame==100);
    for(int i=50;i<100;++i) f[i].pitch.midi=72;
    notes=pg::segmentNotes(f,.01); CHECK(notes.size()==2); CHECK(std::abs(notes[1].start-.495)<.011);
    CHECK(notes[0].end<=notes[1].start);
    f=frames(200); for(int i=0;i<200;++i) f[i].pitch.midi+=.45*std::sin(2*3.141592653589793*5*i*.01);
    CHECK(pg::segmentNotes(f,.01).size()==1);
    f=frames(100); for(int i=40;i<60;++i) f[i].pitch={}; CHECK(pg::segmentNotes(f,.01).size()==2);
    f=frames(100); f[40].pitch={}; f[41].pitch={}; CHECK(pg::segmentNotes(f,.01).size()==1);
    f=frames(100,69,.2); CHECK(pg::segmentNotes(f,.01).empty());
    CHECK(pg::segmentNotes(frames(4),.01).empty());
    f=frames(160); for(int i=50;i<90;++i) f[i].pitch.midi=69+4.*(i-50)/40;
    for(int i=90;i<160;++i) f[i].pitch.midi=73;
    CHECK(pg::segmentNotes(f,.01).size()<=2);
    // Segmentation cannot change any raw frame.
    const auto before=f[70].pitch.midi; pg::segmentNotes(f,.01); CHECK(f[70].pitch.midi==before);
    std::cout << "stable notes, transitions, vibrato, silence, confidence, gaps and slides passed\n";
}
