#include "PitchAnalysis.h"
#include "NoteSegmenter.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; return 1; } } while(false)
constexpr double pi=3.14159265358979323846;
// Independent Hann-windowed DFT diagnostic, never calls the lag estimator.
double spectrum(const std::vector<float>& x,double sr,double hz) {
    double real=0,imag=0;
    for(std::size_t i=0;i<x.size();++i) {
        const double w=.5-.5*std::cos(2*pi*i/(x.size()-1));
        real+=w*x[i]*std::cos(2*pi*hz*i/sr); imag-=w*x[i]*std::sin(2*pi*hz*i/sr);
    }
    return 4*std::hypot(real,imag)/x.size();
}
std::vector<float> signal(double sr,double duration,double hz,double fundamental=0.6,double second=0,double third=0) {
    std::vector<float> x(static_cast<std::size_t>(sr*duration));
    for(std::size_t i=0;i<x.size();++i) { const auto p=2*pi*hz*i/sr; x[i]=float(fundamental*std::sin(p)+second*std::sin(2*p)+third*std::sin(3*p)); }
    return x;
}
std::vector<pg::PitchFrame> analyze(const std::vector<float>& x,double sr) {
    return pg::analyzeSource(sr,x.size(),[&](auto pos,int n,float* dst) { std::copy_n(x.data()+pos,n,dst); return true; });
}
int main() {
    bool failed=false;
    for(double sr:{44100.,48000.,96000.,192000.}) for(double hz:{82.4069,110.,196.,220.,440.,880.}) {
        const auto x=signal(sr,.16,hz); const auto frames=analyze(x,sr);
        CHECK(!frames.empty());
        for(const auto& f:frames) CHECK(f.pitch.voiced && std::abs(f.pitch.midi-pg::frequencyToMidi(hz))<.12);
    }
    for(double hz:{110.,196.,220.}) {
        const auto x=signal(48000,.3,hz,.12,.65,.035);
        const auto fundamental=spectrum(x,48000,hz),second=spectrum(x,48000,2*hz);
        CHECK(fundamental>.11 && second>.64);
        const auto frames=analyze(x,48000);
        std::cout << "weak fundamental " << hz << " Hz; DFT amplitudes " << fundamental << ", " << second
                  << "; raw YIN " << frames[frames.size()/2].pitch.rawHz
                  << "; accepted " << frames[frames.size()/2].pitch.hz << " Hz\n";
        for(const auto& f:frames) {
            CHECK(f.pitch.octaveCorrected);
            CHECK(f.pitch.rawHz>1.9*hz && f.pitch.rawHz<2.1*hz);
            CHECK(std::abs(f.pitch.rawMidi-pg::frequencyToMidi(f.pitch.rawHz))<1e-9);
            CHECK(f.pitch.rawConfidence>=.7 && f.pitch.confidence>f.pitch.rawConfidence);
        }
        for(const auto& f:frames) if(!f.pitch.voiced || std::abs(f.pitch.midi-pg::frequencyToMidi(hz))>.15) failed=true;
    }
    CHECK(!failed);
    // Missing fundamental, with a strong third harmonic: keep true periodicity.
    const auto missing=signal(48000,.25,196,0,.6,.3);
    for(const auto& f:analyze(missing,48000)) CHECK(f.pitch.voiced && std::abs(f.pitch.midi-55)<.15);
    // Continuity alone cannot force a short, real upper octave down.
    auto shortOctave=signal(48000,.2,220); const auto brief=signal(48000,.05,440);
    shortOctave.insert(shortOctave.end(),brief.begin(),brief.end());
    const auto returned=signal(48000,.2,220); shortOctave.insert(shortOctave.end(),returned.begin(),returned.end());
    bool foundUpper=false;
    for(const auto& f:analyze(shortOctave,48000)) if(f.seconds>.22 && f.seconds<.23) {
        CHECK(f.pitch.voiced && std::abs(f.pitch.midi-69)<.15 && !f.pitch.octaveCorrected); foundUpper=true;
    }
    CHECK(foundUpper);
    // Audibly descending notes: lower note has the stronger second harmonic.
    auto x=signal(48000,.4,246.9417); const auto lower=signal(48000,.4,196,.12,.65,.035);
    x.insert(x.end(),lower.begin(),lower.end());
    auto frames=analyze(x,48000); auto notes=pg::segmentNotes(frames,256./48000);
    CHECK(notes.size()==2); CHECK(std::abs(notes[0].continuousMidi-59)<.15); CHECK(std::abs(notes[1].continuousMidi-55)<.15);
    // Real octave transitions must survive.
    x=signal(48000,.4,220); const auto upper=signal(48000,.4,440); x.insert(x.end(),upper.begin(),upper.end());
    notes=pg::segmentNotes(analyze(x,48000),256./48000);
    CHECK(notes.size()==2 && notes[0].midiNote==57 && notes[1].midiNote==69);
    // Continuous phase for vocal-like vibrato and glissando.
    for(bool glide:{false,true}) {
        double phase=0; x.resize(48000);
        for(std::size_t i=0;i<x.size();++i) {
            const double t=double(i)/48000;
            const double midi=57+(glide ? 7*t : .3*std::sin(2*pi*5*t));
            phase+=2*pi*440*std::pow(2.,(midi-69)/12)/48000; x[i]=float(.6*std::sin(phase));
        }
        frames=analyze(x,48000);
        for(const auto& f:frames) {
            const double expected=57+(glide ? 7*f.seconds : .3*std::sin(2*pi*5*f.seconds));
            CHECK(f.pitch.voiced && std::abs(f.pitch.midi-expected)<.22);
        }
    }
    std::mt19937 rng(42); std::uniform_real_distribution<float> noise(-.5f,.5f);
    for(auto& sample:x) sample=noise(rng);
    frames=analyze(x,48000);
    CHECK(pg::segmentNotes(frames,256./48000).empty());
    for(const auto& f:frames) CHECK(!f.pitch.voiced || f.pitch.confidence<.7);
    std::cout << "pitch accuracy, descending notes, real octaves, vibrato, glissando and noise passed\n";
}
