#pragma once
#include "PitchDetector.h"
namespace pg {
struct Note {
    double start=0,end=0;
    int midiNote=0;
    double continuousMidi=0,cents=0,confidence=0;
    std::size_t firstFrame=0,endFrame=0; // half-open indices into original frames
};
struct SegmentationSettings {
    double minimumDuration=.07, maximumGap=.04, minimumConfidence=.7;
    double transitionSemitones=.9, transitionHold=.12, transitionSpread=.5;
};
std::vector<Note> segmentNotes(const std::vector<PitchFrame>&, double framePeriod,
                             const SegmentationSettings& = {});
}
