#include "MidiReaperHost.h"
namespace pg {
struct MidiReaperHost::Binding {
    explicit Binding(Steinberg::IPtr<ReaperHostApplication> value) : host(std::move(value)) {}
    Steinberg::IPtr<ReaperHostApplication> host;
    bool (*validate)(void*,const char*)=nullptr;
    int (*tempoCount)(ReaProject*)=nullptr;
    void (*signature)(ReaProject*,double*,double*)=nullptr;
    int (*selectedCount)(ReaProject*)=nullptr;
    MediaTrack* (*selectedTrack)(ReaProject*,int)=nullptr;
    MediaItem* (*create)(MediaTrack*,double,double,const bool*)=nullptr;
    MediaItem_Take* (*take)(MediaItem*)=nullptr;
    double (*ppq)(MediaItem_Take*,double)=nullptr;
    bool (*note)(MediaItem_Take*,bool,bool,double,double,int,int,int,const bool*)=nullptr;
    void (*sort)(MediaItem_Take*)=nullptr;
    bool (*remove)(MediaTrack*,MediaItem*)=nullptr;
    void (*undoBegin)(ReaProject*)=nullptr;
    void (*undoEnd)(ReaProject*,const char*,int)=nullptr;
    void (*update)()=nullptr;
};
void MidiReaperHost::setIHostApplication(Steinberg::FUnknown* context) {
    std::shared_ptr<Binding> next; ReaperHostApplication* host=nullptr;
    if(context && context->queryInterface(reaperHostIID,reinterpret_cast<void**>(&host))==Steinberg::kResultOk && host) {
        Steinberg::IPtr<ReaperHostApplication> acquired(host,false);
        next=std::make_shared<Binding>(std::move(acquired));
#define PG_API(member,name) next->member=reinterpret_cast<decltype(next->member)>(host->getReaperApi(name))
        PG_API(validate,"ValidatePtr"); PG_API(tempoCount,"CountTempoTimeSigMarkers"); PG_API(signature,"GetProjectTimeSignature2");
        PG_API(selectedCount,"CountSelectedTracks"); PG_API(selectedTrack,"GetSelectedTrack");
        PG_API(create,"CreateNewMIDIItemInProj"); PG_API(take,"GetActiveTake"); PG_API(ppq,"MIDI_GetPPQPosFromProjTime");
        PG_API(note,"MIDI_InsertNote"); PG_API(sort,"MIDI_Sort"); PG_API(remove,"DeleteTrackMediaItem");
        PG_API(undoBegin,"Undo_BeginBlock2"); PG_API(undoEnd,"Undo_EndBlock2"); PG_API(update,"UpdateArrange");
#undef PG_API
    }
    const std::lock_guard<std::mutex> guard(mutex); binding.swap(next);
}
std::shared_ptr<MidiReaperHost::Binding> MidiReaperHost::snapshot() const {
    const std::lock_guard<std::mutex> guard(mutex); return binding;
}
ReaProject* MidiReaperHost::projectFor(const std::shared_ptr<Binding>& current) {
    auto* manager=juce::MessageManager::getInstanceWithoutCreating();
    if(!manager || !manager->isThisTheMessageThread() || !current || !current->validate) return nullptr;
    auto* project=static_cast<ReaProject*>(current->host->getReaperParent(3));
    return project && current->validate(project,"ReaProject*") ? project : nullptr;
}
std::optional<double> MidiReaperHost::constantTempo() const {
    const auto current=snapshot(); auto* project=projectFor(current);
    // A conservative gate: even pure time-signature markers disable file drag.
    // GetProjectTimeSignature2 explicitly ignores tempo envelopes; it is safe
    // here only because the marker count must be zero.
    if(!project || !current->tempoCount || !current->signature || current->tempoCount(project)!=0) return {};
    double bpm=0,beats=0; current->signature(project,&bpm,&beats);
    try { MidiTempo{bpm}.microseconds(); } catch(...) { return {}; }
    return bpm;
}
juce::Result MidiReaperHost::insert(const std::vector<MidiNote>& notes,const RegionMapping& mapping) const {
    if(!mapping.supported() || notes.empty() || !std::isfinite(mapping.timelineStart+mapping.playbackDuration))
        return juce::Result::fail("No valid single-region notes to insert");
    for(const auto& n:notes) if(n.pitch<0 || n.pitch>127 || !std::isfinite(n.start) || !std::isfinite(n.end)
        || n.start<0 || n.end<=n.start || n.end>mapping.playbackDuration+1e-8)
        return juce::Result::fail("Invalid MIDI note boundaries");
    const auto current=snapshot(); auto* project=projectFor(current);
    if(!project || !current->selectedCount || !current->selectedTrack || !current->create || !current->take || !current->ppq
        || !current->note || !current->sort || !current->remove || !current->undoBegin || !current->undoEnd || !current->update)
        return juce::Result::fail("Native insertion unavailable: REAPER VST3 MIDI APIs/project required");
    if(current->selectedCount(project)!=1) return juce::Result::fail("Select exactly one destination track in REAPER");
    auto* track=current->selectedTrack(project,0);
    if(!track || !current->validate(track,"MediaTrack*")) return juce::Result::fail("Destination track is unavailable");
    current->undoBegin(project);
    const bool seconds=false,noSort=true;
    auto* item=current->create(track,mapping.timelineStart,mapping.timelineStart+mapping.playbackDuration,&seconds);
    auto* take=item ? current->take(item) : nullptr;
    juce::String error;
    if(!take) error="REAPER could not create an empty MIDI item";
    for(const auto& n:notes) {
        if(error.isNotEmpty()) break;
        const auto start=current->ppq(take,mapping.timelineStart+n.start);
        const auto end=current->ppq(take,mapping.timelineStart+n.end);
        if(!std::isfinite(start) || !std::isfinite(end) || end<=start
            || !current->note(take,false,false,start,end,0,n.pitch,100,&noSort)) error="REAPER could not insert MIDI notes";
    }
    if(error.isEmpty()) current->sort(take);
    else if(item && !current->remove(track,item)) error+="; cleanup failed: undo the incomplete MIDI item";
    current->undoEnd(project,error.isEmpty() ? "PitchGarden MIDI: insert recognized notes" : "PitchGarden MIDI: failed insertion",-1);
    current->update();
    return error.isEmpty() ? juce::Result::ok() : juce::Result::fail(error);
}
}
