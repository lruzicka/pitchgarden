#include "MidiReaperHost.h"
#include <cstring>
#include <iostream>
#include <thread>
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
namespace {
const auto ui=std::this_thread::get_id();
struct State {
    int markers=0,selected=1,created=0,removed=0,begun=0,ended=0,sorted=0;
    bool valid=true,failNote=false;
    double bpm=90,itemStart=0,itemEnd=0;
    std::vector<std::pair<double,double>> inserted;
} state;
ReaProject* project=reinterpret_cast<ReaProject*>(&state);
MediaTrack* track=reinterpret_cast<MediaTrack*>(&state.selected);
MediaItem* item=reinterpret_cast<MediaItem*>(&state.created);
MediaItem_Take* take=reinterpret_cast<MediaItem_Take*>(&state.inserted);
bool validate(void* ptr,const char*) { CHECK(std::this_thread::get_id()==ui); return state.valid && (ptr==project || ptr==track); }
int markers(ReaProject* p) { CHECK(p==project); return state.markers; }
void signature(ReaProject* p,double* bpm,double* beats) { CHECK(p==project); *bpm=state.bpm; *beats=4; }
int selected(ReaProject* p) { CHECK(p==project); return state.selected; }
MediaTrack* getTrack(ReaProject* p,int index) { CHECK(p==project && index==0); return track; }
MediaItem* create(MediaTrack* t,double start,double end,const bool* qn) { CHECK(t==track && qn && !*qn); ++state.created; state.itemStart=start; state.itemEnd=end; state.inserted.clear(); return item; }
MediaItem_Take* getTake(MediaItem* i) { CHECK(i==item); return take; }
double ppq(MediaItem_Take* t,double time) { CHECK(t==take); return (time-12)*960+std::max(0.,time-13)*480; }
bool insert(MediaItem_Take* t,bool select,bool mute,double start,double end,int channel,int pitch,int velocity,const bool* noSort) {
    CHECK(std::this_thread::get_id()==ui && t==take && !select && !mute && channel==0 && pitch==60 && velocity==100 && noSort && *noSort);
    if(state.failNote) return false;
    state.inserted.emplace_back(start,end); return true;
}
void sort(MediaItem_Take* t) { CHECK(t==take); ++state.sorted; }
bool remove(MediaTrack* t,MediaItem* i) { CHECK(t==track && i==item); ++state.removed; state.inserted.clear(); return true; }
void begin(ReaProject* p) { CHECK(p==project); ++state.begun; }
void end(ReaProject* p,const char*,int) { CHECK(p==project); ++state.ended; }
void update() {}
struct Host final : pg::ReaperHostApplication {
    int refs=1; bool missing=false;
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID id,void** out) override {
        *out=nullptr; if(std::memcmp(id,pg::reaperHostIID,16)) return Steinberg::kNoInterface;
        *out=this; addRef(); return Steinberg::kResultOk;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return ++refs; }
    Steinberg::uint32 PLUGIN_API release() override { return --refs; }
    void* PLUGIN_API getReaperParent(Steinberg::uint32 kind) override { CHECK(kind==3); return project; }
    void* PLUGIN_API reaperExtended(Steinberg::uint32,void*,void*,void*) override { return nullptr; }
    void* PLUGIN_API getReaperApi(Steinberg::CStringA name) override {
        if(missing) return nullptr;
#define API(key,fn) if(!std::strcmp(name,key)) return reinterpret_cast<void*>(&fn)
        API("ValidatePtr",validate); API("CountTempoTimeSigMarkers",markers); API("GetProjectTimeSignature2",signature);
        API("CountSelectedTracks",selected); API("GetSelectedTrack",getTrack); API("CreateNewMIDIItemInProj",create);
        API("GetActiveTake",getTake); API("MIDI_GetPPQPosFromProjTime",ppq); API("MIDI_InsertNote",insert); API("MIDI_Sort",sort);
        API("DeleteTrackMediaItem",remove); API("Undo_BeginBlock2",begin); API("Undo_EndBlock2",end); API("UpdateArrange",update);
#undef API
        return nullptr;
    }
};
}
int main() {
    juce::ScopedJuceInitialiser_GUI gui; Host host; pg::MidiReaperHost bridge;
    const pg::RegionMapping mapping{12,3,3,3}; const std::vector<pg::MidiNote> notes{{60,.25,2.5}};
    CHECK(bridge.insert(notes,mapping).failed() && !bridge.constantTempo());
    bridge.setIHostApplication(&host); CHECK(host.refs==2 && bridge.constantTempo()==90);
    state.markers=1; CHECK(!bridge.constantTempo());
    CHECK(bridge.insert(notes,mapping).wasOk());
    CHECK(state.itemStart==12 && state.itemEnd==15 && state.inserted.size()==1);
    CHECK(state.inserted[0].first==240 && state.inserted[0].second==3120); // nonlinear project tempo conversion
    CHECK(state.begun==1 && state.ended==1 && state.sorted==1);
    state.selected=0; CHECK(bridge.insert(notes,mapping).failed() && state.created==1);
    state.selected=2; CHECK(bridge.insert(notes,mapping).failed() && state.created==1);
    state.selected=1; state.failNote=true; CHECK(bridge.insert(notes,mapping).failed());
    CHECK(state.removed==1 && state.inserted.empty() && state.begun==state.ended);
    state.failNote=false; state.valid=false; CHECK(bridge.insert(notes,mapping).failed()); state.valid=true;
    const int created=state.created;
    std::thread worker([&] { CHECK(bridge.insert(notes,mapping).failed() && !bridge.constantTempo()); }); worker.join(); CHECK(state.created==created);
    host.missing=true; bridge.setIHostApplication(&host); CHECK(bridge.insert(notes,mapping).failed());
    bridge.setIHostApplication(nullptr); CHECK(host.refs==1);
    std::cout << "REAPER MIDI project/track context, nonlinear timing, rollback, tempo gate and UI-thread lifetime passed\n";
}
