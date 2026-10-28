#include "PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace pg {
PitchObservation detectPitch(const float* x, std::size_t n, double sr,
                             double minHz, double maxHz, double threshold)
{
    if (!x || !std::isfinite(sr) || !std::isfinite(minHz) || !std::isfinite(maxHz)
        || !std::isfinite(threshold) || threshold <= 0 || threshold >= 1
        || sr <= 0 || minHz <= 0 || maxHz <= minHz || maxHz >= sr / 2 || n < 32)
        return {};
    const double lowestLag=std::max(2.,std::floor(sr/maxHz));
    const double highestLag=std::min(double(n/2-1),std::ceil(sr/minHz));
    if(!std::isfinite(lowestLag) || lowestLag>=highestLag) return {};
    const auto lo=static_cast<std::size_t>(lowestLag);
    const auto hi=static_cast<std::size_t>(highestLag);
    if (lo >= hi) return {};
    double meanSquare = 0.0;
    for (std::size_t i = 0; i < n; ++i) meanSquare += double(x[i]) * x[i];
    meanSquare /= n;
    if (!std::isfinite(meanSquare)) return {};
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
    const auto observation = [&](std::size_t lag) {
        double refined = double(lag);
        if (lag > 1 && lag < hi) {
            const double a = cmnd[lag - 1], b = cmnd[lag], c = cmnd[lag + 1];
            const double denom = a - 2 * b + c;
            if (std::abs(denom) > 1.0e-12)
                refined += std::clamp(0.5 * (a - c) / denom, -1.0, 1.0);
        }
        const double hz = sr / refined;
        return PitchObservation{hz, 69.0 + 12.0 * std::log2(hz / 440.0),
                               std::clamp(1.0 - cmnd[lag], 0.0, 1.0), true};
    };
    const auto raw = observation(best);
    auto selected = raw;
    // A strong second harmonic can cross YIN's threshold at half the true
    // period. Demand a substantially better full period AND odd-harmonic
    // spectral evidence. A pure high sine has equally good multiples and is
    // deliberately left alone. This is offline, never on the audio thread.
    const auto begin = std::max(best + 1, static_cast<std::size_t>(best * 1.9));
    const auto end = std::min(hi, static_cast<std::size_t>(best * 2.1));
    if (begin <= end) {
        const auto longer = static_cast<std::size_t>(std::min_element(cmnd.begin()+begin, cmnd.begin()+end+1)-cmnd.begin());
        if (cmnd[best]-cmnd[longer] > .02 && cmnd[longer] < .2*cmnd[best]) {
            const auto candidate = observation(longer);
            const auto amplitude = [&](double hz) {
                double real=0,imag=0,weight=0;
                for (std::size_t i=0;i<n;++i) {
                    const double w=.5-.5*std::cos(2*3.14159265358979323846*i/(n-1));
                    const double phase=2*3.14159265358979323846*hz*i/sr;
                    real+=w*x[i]*std::cos(phase); imag-=w*x[i]*std::sin(phase); weight+=w;
                }
                return 2*std::hypot(real,imag)/weight;
            };
            if (std::abs(raw.midi-candidate.midi-12)<.5
                && std::max(amplitude(candidate.hz),amplitude(3*candidate.hz)) > .12*std::sqrt(meanSquare)) {
                selected=candidate; selected.octaveCorrected=true;
            }
        }
    }
    selected.rawHz=raw.hz; selected.rawMidi=raw.midi; selected.rawConfidence=raw.confidence;
    return selected;
}

void confirmOctaveCorrections(std::vector<PitchFrame>& frames, double period) {
    // Decide from an immutable copy so traversal direction cannot affect results.
    const auto candidates=frames;
    for (std::size_t i=0;i<frames.size();++i) {
        auto& p=frames[i].pitch;
        if (!p.octaveCorrected) continue;
        int support=0;
        const auto end=std::min(frames.size(),i+3);
        for (auto j=(i>2 ? i-2 : 0);j<end;++j) {
            const auto& neighbor=candidates[j];
            if (j!=i && neighbor.pitch.voiced && neighbor.pitch.confidence>=.7
                && std::abs(neighbor.seconds-frames[i].seconds)<=2.5*period
                && std::abs(neighbor.pitch.midi-p.midi)<.7) ++support;
        }
        if (support<2) {
            p.hz=p.rawHz; p.midi=p.rawMidi; p.confidence=p.rawConfidence; p.octaveCorrected=false;
        }
    }
}

std::vector<PitchFrame> analyze(const std::vector<float>& audio, double sr,
                                std::size_t window, std::size_t hop)
{
    if (!std::isfinite(sr) || sr <= 0 || window < 32 || hop == 0)
        throw std::invalid_argument("Invalid analysis settings");
    std::vector<PitchFrame> result;
    for (std::size_t i = 0; i <= audio.size() && window <= audio.size() - i; i += hop) {
        result.push_back({double(i + window / 2) / sr,
                          detectPitch(audio.data() + i, window, sr)});
        if (i > audio.size() - window - std::min(hop, audio.size() - window)) break;
    }
    confirmOctaveCorrections(result,double(hop)/sr);
    return result;
}
}
