#pragma once
#include <cstddef>
#include <vector>

namespace pg {
struct PitchObservation {
    double hz = 0.0;
    double midi = 0.0;
    double confidence = 0.0;
    bool voiced = false;
    // Original first-minimum YIN result, retained even when corrected below.
    double rawHz = 0.0, rawMidi = 0.0, rawConfidence = 0.0;
    bool octaveCorrected = false;
};

// Normalized difference YIN estimator for monophonic material.
// Offline analysis only. Not suitable for use in a real-time callback.
PitchObservation detectPitch(const float* samples, std::size_t length,
                             double sampleRate, double minHz = 70.0,
                             double maxHz = 1000.0,
                             double threshold = 0.12);

struct PitchFrame {
    double seconds;
    PitchObservation pitch;
};
// Reject isolated signal-supported corrections without local temporal support.
// Never manufactures a pitch for an unvoiced/low-confidence frame.
void confirmOctaveCorrections(std::vector<PitchFrame>&, double framePeriod);
std::vector<PitchFrame> analyze(const std::vector<float>& audio, double sampleRate,
                                std::size_t window = 2048, std::size_t hop = 256);
}
