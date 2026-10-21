#pragma once
#include "JuceHeader.h"
class PitchGardenRegion final : public juce::ARAPlaybackRegion {
public:
    PitchGardenRegion(juce::ARAAudioModification* modification,ARA::ARAPlaybackRegionHostRef hostRef)
        : ARAPlaybackRegion(modification,hostRef),identity(nextIdentity.fetch_add(1)) {}
    const std::uint64_t identity;
private:
    inline static std::atomic<std::uint64_t> nextIdentity{1};
};
