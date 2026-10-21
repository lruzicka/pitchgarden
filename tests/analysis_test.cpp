#include "PitchAnalysis.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; return 1; } } while (false)
int main() {
    CHECK(std::abs(pg::frequencyToMidi(440) - 69) < 1e-9);
    CHECK(!std::isfinite(pg::frequencyToMidi(0)));
    CHECK(!std::isfinite(pg::frequencyToMidi(std::numeric_limits<double>::infinity())));
    CHECK(std::abs(pg::frameTime(256, 2048, 48000) - 1280.0/48000) < 1e-12);
    pg::RegionMapping map{12, 3, 5, 5};
    CHECK(map.supported()); CHECK(map.toTimeline(4) == 13); CHECK(map.toSource(13) == 4);
    CHECK(!pg::RegionMapping({0,0,2,3}).supported());
    for (double sr : {44100.,48000.,96000.}) {
        for (double hz : {82.4069,220.,440.,880.}) {
            pg::AnalysisSettings cfg;
            const auto window = cfg.windowFor(sr);
            std::vector<float> wave(window + 512);
            for (std::size_t i=0; i<wave.size(); ++i) wave[i]=float(.7*std::sin(2*3.141592653589793*hz*i/sr));
            auto result=pg::analyzeSource(sr, wave.size(), [&](std::int64_t pos, int n, float* dst) {
                std::copy_n(wave.data()+pos,n,dst); return true;
            }, cfg);
            CHECK(result.size()==3); CHECK(result[0].pitch.voiced);
            CHECK(std::abs(result[0].pitch.midi-pg::frequencyToMidi(hz))<.12);
            CHECK(std::abs(result[0].seconds-double(window/2)/sr)<1e-12);
        }
    }
    CHECK(!pg::detectPitch(nullptr,2048,48000).voiced);
    std::vector<float> shortWave(12,1); CHECK(!pg::detectPitch(shortWave.data(),12,48000).voiced);
    CHECK(!pg::detectPitch(shortWave.data(),12,std::numeric_limits<double>::quiet_NaN()).voiced);
    auto silence=pg::analyzeSource(48000,4096,[](auto,int n,float* dst) { std::fill_n(dst,n,0.f); return true; });
    CHECK(!silence.empty()); for (auto& f:silence) CHECK(!f.pitch.voiced);
    CHECK(pg::analyzeSource(48000,12,[](auto,int,float*) { return false; }).empty());
    bool failed=false; try { pg::analyzeSource(48000,4096,[](auto,int,float*) { return false; }); }
    catch (const std::runtime_error&) { failed=true; } CHECK(failed);
    bool invalid=false; try { pg::analyzeSource(0,100,{},{}); } catch(const std::invalid_argument&) { invalid=true; } CHECK(invalid);
    std::cout << "analysis conversions, mapping and reference pitches passed\n";
}
