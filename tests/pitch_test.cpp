#include "PitchDetector.h"
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    constexpr double sr = 48000.0;
    std::vector<float> wave(4096);
    for (std::size_t i = 0; i < wave.size(); ++i)
        wave[i] = 0.7f * std::sin(2.0 * 3.141592653589793 * 440.0 * i / sr);
    auto a4 = pg::detectPitch(wave.data(), wave.size(), sr);
    if (!a4.voiced || std::abs(a4.hz - 440.0) > 2.0 || std::abs(a4.midi - 69.0) > 0.1) {
        std::cerr << "A4 detection failed: " << a4.hz << " Hz\n";
        return 1;
    }
    std::vector<float> silence(4096, 0.0f);
    if (pg::detectPitch(silence.data(), silence.size(), sr).voiced) return 2;
    auto frames = pg::analyze(wave, sr, 2048, 256);
    if (frames.empty() || !frames.front().pitch.voiced) return 3;
    std::cout << "OK: A4=" << a4.hz << " Hz, frames=" << frames.size() << '\n';
}
