#include "PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace pg {
PitchObservation detectPitch(const float* x, std::size_t n, double sr,
                             double minHz, double maxHz, double threshold)
{
    if (!x || sr <= 0 || minHz <= 0 || maxHz <= minHz || n < 32)
        return {};
    const auto lo = std::max<std::size_t>(2, static_cast<std::size_t>(sr / maxHz));
    const auto hi = std::min<std::size_t>(n / 2 - 1, static_cast<std::size_t>(std::ceil(sr / minHz)));
    if (lo >= hi) return {};
    double meanSquare = 0.0;
    for (std::size_t i = 0; i < n; ++i) meanSquare += double(x[i]) * x[i];
    meanSquare /= n;
    if (meanSquare < 1.0e-7) return {};

    std::vector<double> d(hi + 1, 0.0), cmnd(hi + 1, 1.0);
    for (std::size_t tau = 1; tau <= hi; ++tau) {
        double sum = 0;
        for (std::size_t j = 0; j < n - hi; ++j) {
            auto delta = double(x[j]) - x[j + tau];
            sum += delta * delta;
        }
        d[tau] = sum;
    }
    double running = 0.0;
    for (std::size_t tau = 1; tau <= hi; ++tau) {
        running += d[tau];
        cmnd[tau] = running > 1.0e-20 ? d[tau] * tau / running : 1.0;
    }
    std::size_t best = 0;
    for (std::size_t tau = lo; tau <= hi; ++tau) {
        if (cmnd[tau] < threshold) {
            while (tau + 1 <= hi && cmnd[tau + 1] < cmnd[tau]) ++tau;
            best = tau;
            break;
        }
    }
    if (!best) {
        best = static_cast<std::size_t>(std::min_element(cmnd.begin() + lo, cmnd.begin() + hi + 1) - cmnd.begin());
        if (cmnd[best] > 0.25) return {};
    }
    double refined = double(best);
    if (best > 1 && best < hi) {
        const double a = cmnd[best - 1], b = cmnd[best], c = cmnd[best + 1];
        const double denom = a - 2 * b + c;
        if (std::abs(denom) > 1.0e-12)
            refined += std::clamp(0.5 * (a - c) / denom, -1.0, 1.0);
    }
    if (refined <= 0) return {};
    const double hz = sr / refined;
    return {hz, 69.0 + 12.0 * std::log2(hz / 440.0),
            std::clamp(1.0 - cmnd[best], 0.0, 1.0), true};
}

std::vector<PitchFrame> analyze(const std::vector<float>& audio, double sr,
                                std::size_t window, std::size_t hop)
{
    if (sr <= 0 || window < 32 || hop == 0)
        throw std::invalid_argument("Invalid analysis settings");
    std::vector<PitchFrame> result;
    for (std::size_t i = 0; i <= audio.size() && window <= audio.size() - i; i += hop) {
        result.push_back({double(i + window / 2) / sr,
                          detectPitch(audio.data() + i, window, sr)});
        if (i > audio.size() - window - std::min(hop, audio.size() - window)) break;
    }
    return result;
}
}
