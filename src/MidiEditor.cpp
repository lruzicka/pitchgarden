#include "MidiEditor.h"
MidiRegionSnapshot collectMidiRegion(juce::ARAEditorView* view) {
    MidiRegionSnapshot value;
    if(!view) { value.message="Open PitchGarden MIDI as an ARA effect on one recorded audio item"; return value; }
    auto* dc=view->getDocumentController();
    if(dc->isHostEditingDocument()) { value.message="Updating audio selection..."; return value; }
    auto regions=view->getViewSelection().getEffectivePlaybackRegions<juce::ARAPlaybackRegion>();
    if(regions.empty()) for(auto* sequence:dc->getDocument<juce::ARADocument>()->getRegionSequences()) {
        const auto& hidden=view->getHiddenRegionSequences();
        if(std::find(hidden.begin(),hidden.end(),sequence)==hidden.end())
            for(auto* region:sequence->getPlaybackRegions()) regions.push_back(region);
    }
    if(regions.size()!=1) {
        value.message=regions.empty() ? "Waiting for one ARA audio item" : "Select exactly one audio item; multi-region export is not supported"; return value;
    }
    auto* region=regions.front();
    value.mapping=PitchGardenPlayback::mapping(*region);
    if(!value.mapping.supported() || region->isTimestretchEnabled() || region->isTimeStretchReflectingTempo()) {
        value.message="Time-stretched/rate-transformed items are not supported; use an un-stretched recording"; return value;
    }
    auto* source=region->getAudioModification()->getAudioSource<AnalyzedSource>();
    value.name=source->getName() ? source->getName() : "Audio item";
    value.analysis=source->snapshot(); value.revision=source->revision.load();
    value.identity=static_cast<PitchGardenRegion*>(region)->identity; value.valid=true;
    return value;
}
void MidiNoteView::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff202832));
    if(!data || data->notes.empty()) {
        g.setColour(juce::Colours::lightgrey); g.drawText("Detected notes will appear here",getLocalBounds(),juce::Justification::centred); return;
    }
    int low=127,high=0; for(const auto& n:data->notes) { low=std::min(low,n.pitch); high=std::max(high,n.pitch); }
    low-=2; high=std::max(high+2,low+12);
    const float left=52,top=24,width=float(getWidth())-left-8,height=float(getHeight())-top-8;
    const auto y=[&](double pitch) { return top+height-float((pitch-low)/(high-low))*height; };
    const auto x=[&](double seconds) { return left+float(seconds/data->mapping.playbackDuration)*width; };
    g.setFont(juce::FontOptions(12.f));
    for(int pitch=low;pitch<=high;++pitch) {
        g.setColour(juce::Colour(0xff3b4653)); g.drawHorizontalLine(int(y(pitch-.5)),left,left+width);
        if(pitch%3==0) { g.setColour(juce::Colours::lightgrey); g.drawText(juce::String(pitch),0,int(y(pitch)-7),46,14,juce::Justification::centredRight); }
    }
    for(int i=0;i<=4;++i) {
        const auto time=data->mapping.playbackDuration*i/4;
        g.setColour(juce::Colours::lightgrey); g.drawText(juce::String(time,2)+"s",int(x(time))-20,2,60,18,juce::Justification::centred);
    }
    for(const auto& n:data->notes) {
        juce::Rectangle<float> bounds(x(n.start),y(n.pitch+.4),std::max(1.f,x(n.end)-x(n.start)),std::max(3.f,y(n.pitch-.4)-y(n.pitch+.4)));
        g.setColour(juce::Colour(0xff65bfb0)); g.fillRect(bounds);
    }
}
MidiEditor::MidiEditor(PitchGardenMidiProcessor& owner)
    : AudioProcessorEditor(&owner),AudioProcessorEditorARAExtension(&owner) {
    setWantsKeyboardFocus(false); setMouseClickGrabsKeyboardFocus(false);
    for(auto* c:std::initializer_list<juce::Component*>{&title,&status,&count,&help,&view,&exportButton,&dragButton}) addAndMakeVisible(c);
    title.setText("PitchGarden MIDI",juce::dontSendNotification); title.setFont(juce::FontOptions(26.f));
    help.setText("Read-only notes. Velocity 100, channel 1. MIDI files use an explicit constant tempo.",juce::dontSendNotification);
    for(auto* button:{&exportButton,&dragButton}) { button->setWantsKeyboardFocus(false); button->setMouseClickGrabsKeyboardFocus(false); }
    exportButton.onClick=[this] { chooseExport(); }; dragButton.setEnabled(false);
    setResizable(true,false); setResizeLimits(640,360,1800,1200); setSize(900,540);
    refresh(); startTimerHz(4);
}
void MidiEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff171c22)); }
void MidiEditor::resized() {
    title.setBounds(16,8,getWidth()-32,36); status.setBounds(16,46,getWidth()-32,24);
    count.setBounds(16,72,getWidth()-32,24); view.setBounds(16,104,getWidth()-32,getHeight()-218);
    dragButton.setBounds(16,getHeight()-98,getWidth()-220,50); exportButton.setBounds(getWidth()-190,getHeight()-98,174,32);
    help.setBounds(16,getHeight()-40,getWidth()-32,32);
}
void MidiEditor::refresh() {
    const auto source=collectMidiRegion(getARAEditorView());
    if(!source.valid) { data.reset(); status.setText(source.message,juce::dontSendNotification); }
    else if(source.analysis.state!=pg::AnalysisState::ready || !source.analysis.result) {
        data.reset(); juce::String message;
        switch(source.analysis.state) {
            case pg::AnalysisState::waiting: message="Waiting for host source samples"; break;
            case pg::AnalysisState::analyzing: message="Analyzing "+juce::String(int(source.analysis.progress*100))+"%"; break;
            case pg::AnalysisState::failed: message="Analysis failed: "+juce::String(source.analysis.error); break;
            case pg::AnalysisState::ready: message="No analysis result"; break;
        }
        status.setText(message,juce::dontSendNotification);
    } else {
        const auto& map=source.mapping;
        if(!data || previousResult!=source.analysis.result || previousIdentity!=source.identity || previousRevision!=source.revision
            || !juce::exactlyEqual(previousMapping.timelineStart,map.timelineStart) || !juce::exactlyEqual(previousMapping.sourceStart,map.sourceStart)
            || !juce::exactlyEqual(previousMapping.sourceDuration,map.sourceDuration) || !juce::exactlyEqual(previousMapping.playbackDuration,map.playbackDuration)) {
            const auto converted=pg::convertMidiNotes(source.analysis.result->notes,map);
            auto next=std::make_shared<MidiExportSnapshot>(); next->notes=converted.notes; next->mapping=map; data=next;
            previousResult=source.analysis.result; previousIdentity=source.identity; previousRevision=source.revision; previousMapping=map;
            status.setText(converted.error.empty() ? (converted.notes.empty() ? "No reliable notes in this region" : "Ready: "+source.name)
                                                 : juce::String(converted.error),juce::dontSendNotification);
        }
    }
    const bool ready=data && !data->notes.empty();
    exportButton.setEnabled(ready); count.setText(juce::String(ready ? int(data->notes.size()) : 0)+" recognized notes",juce::dontSendNotification);
    view.setData(data);
}
void MidiEditor::chooseExport() {
    if(!data || data->notes.empty() || choosing) return;
    const auto selected=data; choosing=true;
    chooser=std::make_unique<juce::FileChooser>("Export PitchGarden MIDI",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("PitchGarden MIDI.mid"),"*.mid");
    juce::Component::SafePointer<MidiEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
        [safe,selected](const juce::FileChooser& choice) {
            if(!safe) return;
            safe->choosing=false; const auto file=choice.getResult(); if(file==juce::File{}) return;
            safe->refresh();
            if(safe->data!=selected) { safe->status.setText("Selection changed; export cancelled",juce::dontSendNotification); return; }
            const auto result=pg::writeMidiFile(file.withFileExtension(".mid"),selected->notes,selected->mapping.playbackDuration,selected->tempo);
            safe->status.setText(result.wasOk() ? "Exported "+file.getFileName() : result.getErrorMessage(),juce::dontSendNotification);
        });
}
juce::AudioProcessorEditor* PitchGardenMidiProcessor::createEditor() { return new MidiEditor(*this); }
