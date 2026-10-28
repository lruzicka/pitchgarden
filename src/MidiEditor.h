#pragma once
#include "MidiPlugin.h"
#include "MidiFileExport.h"
struct MidiRegionSnapshot {
    juce::String message,name;
    pg::RegionMapping mapping;
    pg::AnalysisSnapshot analysis;
    std::uint64_t identity=0,revision=0;
    bool valid=false;
};
MidiRegionSnapshot collectMidiRegion(juce::ARAEditorView*);
struct MidiExportSnapshot { std::vector<pg::MidiNote> notes; pg::RegionMapping mapping; pg::MidiTempo tempo; juce::File dragFile; juce::String dragError; };
class MidiNoteView final : public juce::Component {
public:
    void setData(std::shared_ptr<const MidiExportSnapshot> next) { data=std::move(next); repaint(); }
    void paint(juce::Graphics&) override;
private:
    std::shared_ptr<const MidiExportSnapshot> data;
};
class MidiDragArea final : public juce::Component,public juce::SettableTooltipClient {
public:
    std::function<void()> onDrag;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override { started=false; }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(isEnabled() && !started && e.mods.isLeftButtonDown() && e.getDistanceFromDragStart()>6) {
            started=true; if(onDrag) onDrag();
        }
    }
private:
    bool started=false;
};
class MidiEditor final : public juce::AudioProcessorEditor,
                         private juce::AudioProcessorEditorARAExtension,private juce::Timer {
public:
    explicit MidiEditor(PitchGardenMidiProcessor&);
    ~MidiEditor() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();
    std::shared_ptr<const MidiExportSnapshot> exportSnapshot() const { return data; }
    juce::String analysisMessage() const { return status.getText(); }
    juce::AudioProcessorEditorARAExtension* getARAClientExtensions() override { return this; }
private:
    void timerCallback() override { refresh(); }
    void chooseExport();
    void startMidiDrag();
    void insertMidi();
    PitchGardenMidiProcessor& ownerProcessor;
    juce::Label title,status,count,help;
    juce::TextButton exportButton{"Export MIDI"},insertButton{"Insert on selected track"};
    MidiDragArea dragButton;
    MidiNoteView view;
    std::shared_ptr<const MidiExportSnapshot> data;
    std::shared_ptr<const pg::AnalysisResult> previousResult;
    pg::RegionMapping previousMapping;
    std::uint64_t previousIdentity=0,previousRevision=0;
    std::unique_ptr<juce::FileChooser> chooser;
    bool choosing=false,dragInFlight=false,previousFlatTempo=false;
};
