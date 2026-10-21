#include "PitchEditor.h"
#include <algorithm>
#include <cmath>
#include <limits>
using namespace juce;
namespace {
constexpr int left=66,top=76,bottom=26;
String pitchName(int midi) {
    static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return String(names[(midi%12+12)%12])+String(static_cast<int>(std::floor(midi/12.))-1);
}
bool same(const PitchRegionDisplay& a,const PitchRegionDisplay& b) {
    return a.name==b.name && a.supported==b.supported && a.playbackUnavailable==b.playbackUnavailable && exactlyEqual(a.mapping.timelineStart,b.mapping.timelineStart)
        && exactlyEqual(a.mapping.sourceStart,b.mapping.sourceStart) && exactlyEqual(a.mapping.sourceDuration,b.mapping.sourceDuration)
        && exactlyEqual(a.mapping.playbackDuration,b.mapping.playbackDuration) && a.analysis.state==b.analysis.state
        && a.analysis.result==b.analysis.result && a.analysis.error==b.analysis.error
        && int(a.analysis.progress*100)==int(b.analysis.progress*100);
}
}
PitchEditor::PitchEditor(std::function<std::vector<PitchRegionDisplay>()> fn) : collect(std::move(fn)) {
    addAndMakeVisible(fitButton); fitButton.onClick=[this] { fit(); repaint(); };
    addAndMakeVisible(help); help.setText("Wheel: time  |  Shift: pitch  |  Ctrl: time zoom  |  Ctrl+Shift: pitch zoom  |  Click: select",dontSendNotification);
    help.setFont(FontOptions(12.f));
    addAndMakeVisible(status); status.setFont(FontOptions(13.f));
    startTimerHz(10); timerCallback();
}
void PitchEditor::resized() {
    fitButton.setBounds(8,6,48,26); help.setBounds(66,4,getWidth()-74,28);
    status.setBounds(8,36,getWidth()-16,32);
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
void PitchEditor::timerCallback() {
    auto next=collect(); bool changed=next.size()!=regions.size();
    if(!changed) for(std::size_t i=0;i<next.size();++i) if(!same(next[i],regions[i])) { changed=true; break; }
    if(!changed) return;
    bool refit=next.size()!=regions.size();
    if(!refit) for(std::size_t i=0;i<next.size();++i) {
        const auto& a=next[i]; const auto& b=regions[i];
        if(a.name!=b.name || !exactlyEqual(a.mapping.timelineStart,b.mapping.timelineStart)
            || !exactlyEqual(a.mapping.sourceStart,b.mapping.sourceStart)
            || !exactlyEqual(a.mapping.sourceDuration,b.mapping.sourceDuration)
            || (a.analysis.result && a.analysis.result!=b.analysis.result)) { refit=true; break; }
    }
    if(refit) fitted=false;
    regions=std::move(next); selectedRegion=selectedNote=-1; detail={};
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
            case pg::AnalysisState::ready:
                ready=true;
                text+=String(int(r.analysis.result->notes.size()))+" source notes";
                if(r.analysis.result->notes.empty()) text+=" (no stable voiced notes)";
                break;
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
            Path curve; bool pen=false;
            for(;frame!=result.frames.end() && frame->seconds<=sourceEnd;++frame) {
                if(frame->seconds<sourceBegin || !frame->pitch.voiced || !std::isfinite(frame->pitch.midi)) { pen=false; continue; }
                const auto px=x(r.mapping.toTimeline(frame->seconds)),py=y(frame->pitch.midi);
                if(pen) curve.lineTo(px,py); else curve.startNewSubPath(px,py);
                pen=true;
            }
            g.setColour(Colours::white); g.strokePath(curve,PathStrokeType(1.4f));
        }
    }
    g.setColour(Colours::lightgrey);
    const String ranges="Time "+String(timeStart,2)+"–"+String(timeStart+timeSpan,2)+" s   |   MIDI "+String(pitchLow,1)+"–"+String(pitchLow+pitchSpan,1);
    g.drawText(detail.isNotEmpty() ? detail+"   |   "+ranges : ranges,8,getHeight()-bottom+3,getWidth()-16,bottom-4,Justification::left);
}
void PitchEditor::mouseDown(const MouseEvent& e) {
    if(e.y<top || e.x<left) return;
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
            break;
        }
    }
    repaint();
}
void PitchEditor::mouseWheelMove(const MouseEvent& e,const MouseWheelDetails& wheel) {
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
