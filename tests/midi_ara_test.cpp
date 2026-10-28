#include "MidiEditor.h"
#include <ARA_Library/Dispatch/ARAHostDispatch.h>
#include <ARA_Library/Dispatch/ARAHostDispatch.cpp>
#include <chrono>
#include <iostream>
#include <thread>
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
struct Audio { double rate=48000; };
struct Host final : ARA::Host::AudioAccessControllerInterface,ARA::Host::ArchivingControllerInterface {
    const std::thread::id ui=std::this_thread::get_id();
    int readers=0;
    ARA::ARAAudioReaderHostRef createAudioReaderForSource(ARA::ARAAudioSourceHostRef ref,bool use64) noexcept override {
        CHECK(std::this_thread::get_id()==ui && !use64); ++readers; return reinterpret_cast<ARA::ARAAudioReaderHostRef>(ref);
    }
    bool readAudioSamples(ARA::ARAAudioReaderHostRef ref,ARA::ARASamplePosition start,ARA::ARASampleCount count,void* const buffers[]) noexcept override {
        CHECK(std::this_thread::get_id()!=ui);
        const auto rate=reinterpret_cast<Audio*>(ref)->rate;
        const double frequencies[]={110,146.8324,196,246.9417,329.6276};
        for(ARA::ARASampleCount i=0;i<count;++i) {
            const double time=double(start+i)/rate; const int index=int(time/.4);
            static_cast<float*>(buffers[0])[i]=time>=0 && index<5 && time-index*.4<.3
                ? float(.6*std::sin(2*3.141592653589793*frequencies[index]*time)) : 0.f;
        }
        return true;
    }
    void destroyAudioReader(ARA::ARAAudioReaderHostRef) noexcept override { CHECK(std::this_thread::get_id()==ui); --readers; }
    ARA::ARAPersistentID getDocumentArchiveID(ARA::ARAArchiveReaderHostRef) noexcept override { return "midi-test"; }
    ARA::ARASize getArchiveSize(ARA::ARAArchiveReaderHostRef) noexcept override { return 0; }
    bool readBytesFromArchive(ARA::ARAArchiveReaderHostRef,ARA::ARASize,ARA::ARASize,ARA::ARAByte[]) noexcept override { return false; }
    bool writeBytesToArchive(ARA::ARAArchiveWriterHostRef,ARA::ARASize,ARA::ARASize,const ARA::ARAByte[]) noexcept override { return true; }
    void notifyDocumentArchivingProgress(float) noexcept override {}
    void notifyDocumentUnarchivingProgress(float) noexcept override {}
};
bool ready(AnalyzedSource* source) {
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(source->snapshot().state==pg::AnalysisState::analyzing && std::chrono::steady_clock::now()<until) std::this_thread::yield();
    return source->snapshot().state==pg::AnalysisState::ready;
}
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    auto* factory=juce::ARADocumentControllerSpecialisation::createARAFactory<PitchGardenMidiDocumentController>();
    CHECK(std::strcmp(factory->factoryID,"org.pitchgarden.midi.arafactory.0.1.0")==0);
    CHECK(std::strcmp(factory->documentArchiveID,"org.pitchgarden.midi.aradocumentarchive.1")==0);
    ARA::ARAAssertFunction assertFunction=nullptr;
    ARA::ARAInterfaceConfiguration config{sizeof(config),ARA::kARAAPIGeneration_2_0_Final,&assertFunction}; factory->initializeARAWithConfiguration(&config);
    Host host; Audio audio; ARA::Host::DocumentControllerHostInstance hostInstance(&host,&host);
    ARA::ARADocumentProperties document{sizeof(document),"MIDI test"};
    const auto* instance=factory->createDocumentControllerWithDocument(&hostInstance,&document); ARA::Host::DocumentController controller(instance);
    {
        PitchGardenMidiProcessor processor; CHECK(processor.getName()=="PitchGarden MIDI");
        processor.getARAClientExtensions()->bindToARA(instance->documentControllerRef,
            ARA::kARAPlaybackRendererRole|ARA::kARAEditorRendererRole|ARA::kARAEditorViewRole,ARA::kARAEditorViewRole);
        MidiEditor editor(processor); editor.refresh(); CHECK(!editor.exportSnapshot());
        controller.beginEditing();
        ARA::ARAMusicalContextProperties cp{}; cp.structSize=sizeof(cp); cp.name="Test";
        auto context=controller.createMusicalContext(reinterpret_cast<ARA::ARAMusicalContextHostRef>(&host),&cp);
        ARA::ARARegionSequenceProperties sp{}; sp.structSize=sizeof(sp); sp.musicalContextRef=context;
        auto sequence=controller.createRegionSequence(reinterpret_cast<ARA::ARARegionSequenceHostRef>(&host),&sp);
        ARA::ARAAudioSourceProperties ap{}; ap.structSize=sizeof(ap); ap.name="Five notes with pauses"; ap.persistentID="midi-test-source";
        ap.sampleCount=96000; ap.sampleRate=48000; ap.channelCount=1;
        auto sourceRef=controller.createAudioSource(reinterpret_cast<ARA::ARAAudioSourceHostRef>(&audio),&ap);
        ARA::ARAAudioModificationProperties mp{}; mp.structSize=sizeof(mp); mp.persistentID="midi-mod";
        auto modification=controller.createAudioModification(sourceRef,reinterpret_cast<ARA::ARAAudioModificationHostRef>(&host),&mp);
        ARA::ARAPlaybackRegionProperties rp{}; rp.structSize=sizeof(rp); rp.regionSequenceRef=sequence;
        rp.startInPlaybackTime=12; rp.durationInPlaybackTime=rp.durationInModificationTime=2;
        auto region=controller.createPlaybackRegion(modification,reinterpret_cast<ARA::ARAPlaybackRegionHostRef>(&host),&rp);
        controller.endEditing(); controller.enableAudioSourceSamplesAccess(sourceRef,true);
        auto* source=static_cast<AnalyzedSource*>(ARA::PlugIn::fromRef(sourceRef)); CHECK(ready(source));
        editor.refresh(); auto full=editor.exportSnapshot(); CHECK(full && full->notes.size()==5);
        const int pitches[]={45,50,55,59,64}; for(int i=0;i<5;++i) CHECK(full->notes[std::size_t(i)].pitch==pitches[i]);
        controller.beginEditing(); rp.startInPlaybackTime=30; rp.startInModificationTime=.45;
        rp.durationInPlaybackTime=rp.durationInModificationTime=.95; controller.updatePlaybackRegionProperties(region,&rp); controller.endEditing();
        editor.refresh(); auto trimmed=editor.exportSnapshot(); CHECK(trimmed && trimmed!=full && trimmed->mapping.timelineStart==30 && trimmed->notes.size()==3);
        for(const auto& n:trimmed->notes) CHECK(n.start>=0 && n.end<=.95+1e-9);
        controller.beginEditing();
        auto second=controller.createPlaybackRegion(modification,reinterpret_cast<ARA::ARAPlaybackRegionHostRef>(&audio),&rp); controller.endEditing();
        editor.refresh(); CHECK(!editor.exportSnapshot() && editor.analysisMessage().contains("exactly one"));
        controller.beginEditing(); controller.destroyPlaybackRegion(second); controller.endEditing();
        controller.enableAudioSourceSamplesAccess(sourceRef,false); editor.refresh(); CHECK(!editor.exportSnapshot());
        controller.beginEditing(); audio.rate=44100; ap.sampleRate=44100; ap.sampleCount=88200;
        controller.updateAudioSourceProperties(sourceRef,&ap); controller.endEditing(); controller.enableAudioSourceSamplesAccess(sourceRef,true); CHECK(ready(source));
        editor.refresh(); CHECK(editor.exportSnapshot() && editor.exportSnapshot()->notes.size()==3);
        auto image=editor.createComponentSnapshot(editor.getLocalBounds());
        auto path=juce::File::getCurrentWorkingDirectory().getChildFile("pitchgarden-midi-editor.png");
        if(auto stream=path.createOutputStream()) juce::PNGImageFormat().writeImageToStream(image,*stream);
        controller.enableAudioSourceSamplesAccess(sourceRef,false); editor.refresh(); CHECK(!editor.exportSnapshot());
        controller.beginEditing(); controller.destroyPlaybackRegion(region); controller.destroyAudioModification(modification);
        controller.destroyAudioSource(sourceRef); controller.destroyRegionSequence(sequence); controller.destroyMusicalContext(context); controller.endEditing();
    }
    controller.destroyDocumentController(); CHECK(host.readers==0); factory->uninitializeARA();
    std::cout << "MIDI ARA identity, five-note source, pauses, offsets, trims, multi-region rejection, 48/44.1k replacement and shutdown passed\n";
}
