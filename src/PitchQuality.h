#pragma once
#include "PitchDetector.h"
namespace pg {
enum class PitchQuality { unvoiced,reliable,lowConfidence,octaveExcursion };
struct PitchQualitySettings {
    double minimumConfidence=.7,maximumExcursion=.08,octaveTolerance=.65,returnTolerance=.7;
};
// Annotates uncertainty only: never changes the raw pitch or forces an octave.
std::vector<PitchQuality> classifyPitchQuality(const std::vector<PitchFrame>&,
    double framePeriod,const PitchQualitySettings& = {});
}
