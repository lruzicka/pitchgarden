#pragma once
#include "ReaperCursorControl.h"
#include "MidiConversion.h"
class MediaTrack;
class MediaItem;
class MediaItem_Take;
namespace pg {
// MIDI-plugin-only bridge. No cursor/transport or original-item changes.
class MidiReaperHost final : public juce::VST3ClientExtensions {
public:
    void setIHostApplication(Steinberg::FUnknown*) override;
    std::optional<double> constantTempo() const;
    juce::Result insert(const std::vector<MidiNote>&,const RegionMapping&) const;
private:
    struct Binding;
    std::shared_ptr<Binding> snapshot() const;
    static ReaProject* projectFor(const std::shared_ptr<Binding>&);
    mutable std::mutex mutex;
    std::shared_ptr<Binding> binding;
};
}
