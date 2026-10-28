#include "PitchEditor.h"
#include <algorithm>
#include <cmath>
#include <limits>
using namespace juce;
namespace {
constexpr int left=66,top=100,bottom=26;
String pitchName(int midi) {
    static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return String(names[(midi%12+12)%12])+String(static_cast<int>(std::floor(midi/12.))-1);
}
bool same(const PitchRegionDisplay& a,const PitchRegionDisplay& b) {
    return a.name==b.name && a.identity==b.identity && a.sourceRevision==b.sourceRevision && a.supported==b.supported && a.playbackUnavailable==b.playbackUnavailable && exactlyEqual(a.mapping.timelineStart,b.mapping.timelineStart)
        && exactlyEqual(a.mapping.sourceStart,b.mapping.sourceStart) && exactlyEqual(a.mapping.sourceDuration,b.mapping.sourceDuration)
        && exactlyEqual(a.mapping.playbackDuration,b.mapping.playbackDuration) && a.analysis.state==b.analysis.state
        && a.analysis.result==b.analysis.result && a.analysis.error==b.analysis.error
        && int(a.analysis.progress*100)==int(b.analysis.progress*100);
}
}
PitchEditor::PitchEditor(std::function<std::vector<PitchRegionDisplay>()> fn,
    std::function<pg::TransportSnapshot()> position,PitchAuditionControls preview)
    : collect(std::move(fn)),readTransport(std::move(position)),audition(std::move(preview)) {
    setWantsKeyboardFocus(false); setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(diagnosticsButton);
    diagnosticsButton.setWantsKeyboardFocus(false); diagnosticsButton.setMouseClickGrabsKeyboardFocus(false);
    diagnosticsButton.setTooltip("Hover over the pitch curve: source time, original YIN, accepted pitch and note median");
    diagnosticsButton.onClick=[this] { repaint(); };
    addAndMakeVisible(auditionStatus); auditionStatus.setFont(FontOptions(12.f));
    addAndMakeVisible(followButton); followButton.setToggleState(false,dontSendNotification);
    followButton.setWantsKeyboardFocus(false); followButton.setMouseClickGrabsKeyboardFocus(false);
    followButton.setTooltip("Follow host playback. Manual scrolling/zoom suspends Follow.");
    fitButton.setWantsKeyboardFocus(false); fitButton.setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(fitButton); fitButton.onClick=[this] { suspendFollow(); fit(); repaint(); };
    addAndMakeVisible(help); help.setText("Wheel: time  |  Shift: pitch  |  Ctrl: time zoom  |  Ctrl+Shift: pitch zoom  |  Hold note: audition",dontSendNotification);
    help.setFont(FontOptions(12.f));
    addAndMakeVisible(status); status.setFont(FontOptions(13.f));
    startTimerHz(30); timerCallback();
}
PitchEditor::~PitchEditor() { stopTimer(); stopHeldAudition(); }
void PitchEditor::stopHeldAudition() {
    if(!auditionHeld) return;
    auditionHeld=false;
    if(audition.stop) audition.stop();
}
void PitchEditor::mouseUp(const MouseEvent&) { stopHeldAudition(); }
void PitchEditor::mouseDrag(const MouseEvent& e) {
    if(!e.mods.isLeftButtonDown()) stopHeldAudition();
    updateDiagnostic(e);
}
void PitchEditor::mouseMove(const MouseEvent& e) { updateDiagnostic(e); }
void PitchEditor::visibilityChanged() { if(!isShowing()) stopHeldAudition(); }
void PitchEditor::enablementChanged() { if(!isEnabled()) stopHeldAudition(); }
void PitchEditor::resized() {
    fitButton.setBounds(8,6,48,26); followButton.setBounds(62,6,78,26); diagnosticsButton.setBounds(142,6,108,26); help.setBounds(258,4,getWidth()-266,28);
    status.setBounds(8,34,getWidth()-16,24); auditionStatus.setBounds(8,58,getWidth()-16,24);
}
float PitchEditor::x(double time) const { return float(left+(time-timeStart)/timeSpan*(getWidth()-left)); }
float PitchEditor::y(double pitch) const { return float(getHeight()-bottom-(pitch-pitchLow)/pitchSpan*(getHeight()-top-bottom)); }
Rectangle<float> PitchEditor::noteBounds(const PitchRegionDisplay& r,const pg::Note& n) const {
    const auto start=std::max(n.start,r.mapping.sourceStart);
    const auto end=std::min(n.end,r.mapping.sourceStart+r.mapping.sourceDuration);
    return {x(r.mapping.toTimeline(start)),y(n.midiNote+.42),
        std::max(0.f,x(r.mapping.toTimeline(end))-x(r.mapping.toTimeline(start))),
        std::max(3.f,y(n.midiNote-.42)-y(n.midiNote+.42))};
}
void PitchEditor::fit() {
    double begin=std::numeric_limits<double>::infinity(),end=-begin,low=128,high=0;
    for(auto& r:regions) {
        if(!r.supported) continue;
        begin=std::min(begin,r.mapping.timelineStart); end=std::max(end,r.mapping.timelineStart+r.mapping.playbackDuration);
        if(r.analysis.result) for(auto& n:r.analysis.result->notes)
            if(n.end>r.mapping.sourceStart && n.start<r.mapping.sourceStart+r.mapping.sourceDuration) {
                low=std::min(low,n.continuousMidi); high=std::max(high,n.continuousMidi);
            }
    }
    if(std::isfinite(begin) && end>begin) { timeStart=begin-.05; timeSpan=std::max(.2,end-begin+.1); }
    if(high>=low) { pitchLow=std::floor(low)-3; pitchSpan=std::max(12.,std::ceil(high)-pitchLow+3); }
}
void PitchEditor::suspendFollow() { followButton.setToggleState(false,dontSendNotification); }
void PitchEditor::updateTransport() {
    const auto next=readTransport ? readTransport() : pg::TransportSnapshot{};
    const auto old=transport; transport=next;
    const double followed=pg::followStart(timeStart,timeSpan,transport,followButton.getToggleState());
    if(!exactlyEqual(followed,timeStart)) { timeStart=followed; repaint(); return; }
    if(old.available==next.available && old.playing==next.playing && old.looping==next.looping
        && exactlyEqual(old.projectSeconds,next.projectSeconds)) return;
    const auto repaintCursor=[this](const pg::TransportSnapshot& state) {
        if(!state.available || state.projectSeconds<timeStart || state.projectSeconds>timeStart+timeSpan) return;
        repaint(int(x(state.projectSeconds))-5,top-20,11,getHeight()-top-bottom+20);
    };
    repaintCursor(old); repaintCursor(next);
    repaint(0,getHeight()-bottom,getWidth(),bottom);
}
void PitchEditor::timerCallback() {
    updateTransport();
    // JUCE captures a drag's mouse-up even outside the component. Also poll
    // native button state for lost capture/cancellation and window deactivation.
    if(auditionHeld && (!ModifierKeys::getCurrentModifiersRealtime().isLeftButtonDown()
        || !isShowing() || !isEnabled() || !Process::isForegroundProcess() || transport.playing
        || !audition.available || !audition.available())) stopHeldAudition();
    const auto previewStatus=audition.status ? audition.status() : String("Audition unavailable: no ARA editor-renderer role");
    auditionStatus.setText(previewStatus,dontSendNotification);
    auto next=collect(); bool changed=next.size()!=regions.size();
    if(!changed) for(std::size_t i=0;i<next.size();++i) if(!same(next[i],regions[i])) { changed=true; break; }
    if(!changed) return;
    stopHeldAudition();
    bool refit=next.size()!=regions.size();
    if(!refit) for(std::size_t i=0;i<next.size();++i) {
        const auto& a=next[i]; const auto& b=regions[i];
        if(a.identity!=b.identity || a.sourceRevision!=b.sourceRevision || a.name!=b.name || !exactlyEqual(a.mapping.timelineStart,b.mapping.timelineStart)
            || !exactlyEqual(a.mapping.sourceStart,b.mapping.sourceStart)
            || !exactlyEqual(a.mapping.sourceDuration,b.mapping.sourceDuration)
            || (a.analysis.result && a.analysis.result!=b.analysis.result)) { refit=true; break; }
    }
    if(refit) fitted=false;
    if(refit) { selectedRegion=selectedNote=-1; detail={}; diagnostic={}; }
    regions=std::move(next);
    bool ready=false; String text=regions.empty() ? "Waiting for ARA audio source / region selection" : String{};
    for(auto& r:regions) {
        if(text.isNotEmpty()) text+="  |  ";
        text+=r.name+": ";
        if(!r.supported) { text+="Unavailable: stretched/rate-transformed region"; continue; }
        if(r.playbackUnavailable) text+="Playback cache unavailable; ";
        switch(r.analysis.state) {
            case pg::AnalysisState::waiting: text+="Waiting for source samples"; break;
            case pg::AnalysisState::analyzing: text+="Analyzing "+String(int(r.analysis.progress*100))+"%"; break;
            case pg::AnalysisState::failed: text+="Failed: "+String(r.analysis.error); break;
            case pg::AnalysisState::ready: {
                ready=true;
                text+=String(int(r.analysis.result->notes.size()))+" source notes";
                const auto uncertain=std::count(r.analysis.result->quality.begin(),r.analysis.result->quality.end(),pg::PitchQuality::octaveExcursion);
                if(uncertain>0) text+="; "+String(int(uncertain))+" octave-uncertain frames (orange dots)";
                if(r.analysis.result->notes.empty()) text+=" (no stable voiced notes)";
                break;
            }
        }
    }
    status.setText(text,dontSendNotification);
    if(ready && !fitted) { fit(); fitted=true; }
    repaint();
}
void PitchEditor::paint(Graphics& g) {
    g.fillAll(Colour(0xff171c22));
    const auto plot=Rectangle<int>(left,top,std::max(0,getWidth()-left),std::max(0,getHeight()-top-bottom));
    if(plot.isEmpty()) return;
    g.setFont(FontOptions(12.f));
    for(int midi=int(std::floor(pitchLow));midi<=int(std::ceil(pitchLow+pitchSpan));++midi) {
        const auto row=Rectangle<float>(float(left),y(midi+.5),float(plot.getWidth()),y(midi-.5)-y(midi+.5));
        const auto pc=(midi%12+12)%12;
        const bool black=pc==1 || pc==3 || pc==6 || pc==8 || pc==10;
        g.setColour(black ? Colour(0xff202832) : Colour(0xff2a333e)); g.fillRect(row);
        g.setColour(Colour(0xff495460)); g.drawHorizontalLine(int(y(midi-.5)),float(left),float(getWidth()));
        g.setColour(Colours::lightgrey); g.drawText(pitchName(midi),0,int(y(midi)-7),left-6,14,Justification::centredRight);
    }
    const double rough=timeSpan/8;
    const double magnitude=std::pow(10.,std::floor(std::log10(rough)));
    const double step=magnitude*(rough/magnitude>5 ? 10 : rough/magnitude>2 ? 5 : rough/magnitude>1 ? 2 : 1);
    for(double t=std::ceil(timeStart/step)*step;t<=timeStart+timeSpan;t+=step) {
        g.setColour(Colour(0xff56616c)); g.drawVerticalLine(int(x(t)),float(top),float(getHeight()-bottom));
        g.setColour(Colours::lightgrey); g.drawText(String(t,step<1 ? 2 : 1)+"s",int(x(t))+3,top-16,80,16,Justification::left);
    }
    {
        Graphics::ScopedSaveState save(g); g.reduceClipRegion(plot);
        for(std::size_t rIndex=0;rIndex<regions.size();++rIndex) {
            const auto& r=regions[rIndex]; if(!r.supported || !r.analysis.result) continue;
            const auto& result=*r.analysis.result;
            const double sourceBegin=std::max(r.mapping.sourceStart,r.mapping.toSource(timeStart));
            const double sourceEnd=std::min(r.mapping.sourceStart+r.mapping.sourceDuration,r.mapping.toSource(timeStart+timeSpan));
            if(sourceEnd<=sourceBegin) continue;
            auto note=std::lower_bound(result.notes.begin(),result.notes.end(),sourceBegin,[](auto& n,double t) { return n.end<t; });
            for(;note!=result.notes.end() && note->start<sourceEnd;++note) {
                if(note->midiNote<pitchLow-1 || note->midiNote>pitchLow+pitchSpan+1) continue;
                const auto index=static_cast<int>(note-result.notes.begin());
                auto bounds=noteBounds(r,*note); if(bounds.isEmpty()) continue;
                const bool selected=int(rIndex)==selectedRegion && index==selectedNote;
                g.setColour(selected ? Colour(0xffecc274) : Colour::fromHSV(float(std::fmod(.48+rIndex*.19,1.)),.48f,.72f,.85f)); g.fillRect(bounds.reduced(.5f));
                g.setColour(selected ? Colours::white : Colour(0xffa0bec8)); g.drawRect(bounds,selected ? 2.f : 1.f);
                if(bounds.getWidth()>35) { g.setColour(Colour(0xff111820)); g.drawText(pitchName(note->midiNote),bounds.reduced(3,0),Justification::centredLeft); }
            }
            auto frame=std::lower_bound(result.frames.begin(),result.frames.end(),sourceBegin,[](auto& f,double t) { return f.seconds<t; });
            if(frame!=result.frames.begin()) --frame;
            Path curve; bool pen=false; double previousPitch=0;
            for(;frame!=result.frames.end() && frame->seconds<=sourceEnd;++frame) {
                const auto index=static_cast<std::size_t>(frame-result.frames.begin());
                const auto quality=index<result.quality.size() ? result.quality[index]
                    : (frame->pitch.confidence>=.7 ? pg::PitchQuality::reliable : pg::PitchQuality::lowConfidence);
                if(frame->seconds<sourceBegin || !frame->pitch.voiced || !std::isfinite(frame->pitch.midi)) { pen=false; continue; }
                const auto px=x(r.mapping.toTimeline(frame->seconds)),py=y(frame->pitch.midi);
                if(quality!=pg::PitchQuality::reliable) {
                    // Keep the detected pitch visible, but do not imply a continuous,
                    // certain trajectory through a short octave-like excursion.
                    pen=false;
                    g.setColour(quality==pg::PitchQuality::octaveExcursion ? Colours::orange : Colours::grey);
                    g.fillEllipse(px-1.5f,py-1.5f,3.f,3.f); continue;
                }
                if(pen && std::abs(frame->pitch.midi-previousPitch)<7) curve.lineTo(px,py);
                else curve.startNewSubPath(px,py);
                previousPitch=frame->pitch.midi; pen=true;
            }
            g.setColour(Colours::white); g.strokePath(curve,PathStrokeType(1.4f));
        }
    }
    if(transport.available && transport.projectSeconds>=timeStart && transport.projectSeconds<=timeStart+timeSpan) {
        const float cursorX=x(transport.projectSeconds);
        g.setColour(transport.playing ? Colours::yellow : Colours::orange);
        g.drawVerticalLine(int(cursorX),float(top),float(getHeight()-bottom));
        Path marker; marker.addTriangle(cursorX-4,float(top-5),cursorX+4,float(top-5),cursorX,float(top)); g.fillPath(marker);
    }
    if(diagnosticsButton.getToggleState() && diagnostic.isNotEmpty()) {
        const Rectangle<int> overlay(left+8,top+8,std::max(1,getWidth()-left-16),52);
        g.setColour(Colour(0xee111820)); g.fillRect(overlay);
        g.setColour(Colours::white);
        g.drawFittedText(diagnostic,overlay.reduced(6,4),Justification::centredLeft,2,.7f);
    }
    g.setColour(Colours::lightgrey);
    const String transportText=transport.available ? (transport.playing ? "Playing " : "Stopped ")+String(transport.projectSeconds,3)+" s"+(transport.looping ? " (loop)" : "") : "Host position unavailable";
    const String ranges="Time "+String(timeStart,2)+"–"+String(timeStart+timeSpan,2)+" s   |   MIDI "+String(pitchLow,1)+"–"+String(pitchLow+pitchSpan,1);
    g.drawText(transportText+"   |   "+(detail.isNotEmpty() ? detail+"   |   "+ranges : ranges),8,getHeight()-bottom+3,getWidth()-16,bottom-4,Justification::left);
}
void PitchEditor::mouseDown(const MouseEvent& e) {
    if(!e.mods.isLeftButtonDown()) return;
    stopHeldAudition();
    if(e.y<top || e.x<left || !isEnabled()) return;
    selectedRegion=selectedNote=-1; detail={};
    const double timeline=timeStart+(e.x-left)*timeSpan/std::max(1,getWidth()-left);
    for(std::size_t r=0;r<regions.size();++r) {
        auto& region=regions[r]; if(!region.supported || !region.analysis.result) continue;
        auto& notes=region.analysis.result->notes; const auto t=region.mapping.toSource(timeline);
        auto n=std::lower_bound(notes.begin(),notes.end(),t,[](auto& note,double time) { return note.end<time; });
        if(n!=notes.end() && noteBounds(region,*n).contains(e.position)) {
            selectedRegion=int(r); selectedNote=int(n-notes.begin());
            detail=pitchName(n->midiNote)+"   MIDI "+String(n->continuousMidi,3)+"   "+String(n->cents,1)+" cents   "
                +String(440*std::pow(2.,(n->continuousMidi-69)/12),2)+" Hz   confidence "+String(n->confidence,2);
            if(!transport.playing && audition.available && audition.available() && audition.start)
                auditionHeld=audition.start(region,*n);
            break;
        }
    }
    updateDiagnostic(e);
    repaint();
}
void PitchEditor::updateDiagnostic(const MouseEvent& e) {
    if(!diagnosticsButton.getToggleState() || e.x<left || e.y<top) return;
    diagnostic={};
    const double timeline=timeStart+(e.x-left)*timeSpan/std::max(1,getWidth()-left);
    for(const auto& region:regions) {
        if(!region.supported || !region.analysis.result) continue;
        const double source=region.mapping.toSource(timeline);
        if(source<region.mapping.sourceStart || source>region.mapping.sourceStart+region.mapping.sourceDuration) continue;
        const auto& result=*region.analysis.result;
        auto frame=std::lower_bound(result.frames.begin(),result.frames.end(),source,[](const auto& f,double t) { return f.seconds<t; });
        if(frame!=result.frames.begin() && (frame==result.frames.end() || source-(frame-1)->seconds<frame->seconds-source)) --frame;
        if(frame==result.frames.end()) continue;
        const auto& p=frame->pitch;
        diagnostic="src "+String(frame->seconds,3)+"s raw "+String(p.rawHz,2)+"Hz MIDI "+String(p.rawMidi,3)
            +" conf "+String(p.rawConfidence,2)+"\naccepted MIDI "+String(p.midi,3)+" conf "+String(p.confidence,2)
            +(p.octaveCorrected ? " [octave]" : "");
        const auto note=std::lower_bound(result.notes.begin(),result.notes.end(),frame->seconds,[](const auto& n,double t) { return n.end<t; });
        if(note!=result.notes.end() && note->start<=frame->seconds) diagnostic+=" note "+String(note->continuousMidi,3);
        break;
    }
    repaint(left,top,getWidth()-left,70);
}
void PitchEditor::mouseWheelMove(const MouseEvent& e,const MouseWheelDetails& wheel) {
    suspendFollow();
    const double delta=std::abs(wheel.deltaY)>0 ? wheel.deltaY : wheel.deltaX;
    if(e.mods.isCtrlDown() || e.mods.isCommandDown()) {
        if(e.mods.isShiftDown()) pitchSpan=std::clamp(pitchSpan*std::exp(-delta*2),6.,100.);
        else {
            const auto anchor=timeStart+std::clamp(double(e.x-left)/std::max(1,getWidth()-left),0.,1.)*timeSpan;
            const auto fraction=(anchor-timeStart)/timeSpan;
            timeSpan=std::clamp(timeSpan*std::exp(-delta*2),.1,86400.); timeStart=anchor-fraction*timeSpan;
        }
    } else if(e.mods.isShiftDown()) pitchLow=std::clamp(pitchLow+delta*pitchSpan*.2,-12.,120.);
    else timeStart+=delta*timeSpan*.25;
    repaint();
}
