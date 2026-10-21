#include "PitchGardenARA.h"
#include <ARA_Library/Dispatch/ARAHostDispatch.h>
#include <ARA_Library/Dispatch/ARAHostDispatch.cpp>
#include <chrono>
#include <iostream>
#include <thread>
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
struct TestAudioData { double rate=48000; int channels=1; double hz=440; };
class Host final : public ARA::Host::AudioAccessControllerInterface,public ARA::Host::ArchivingControllerInterface {
public:
    const std::thread::id modelThread=std::this_thread::get_id();
    std::atomic<int> readers{0},reads{0};
    ARA::ARAAudioReaderHostRef createAudioReaderForSource(ARA::ARAAudioSourceHostRef ref,bool use64) noexcept override {
        CHECK(std::this_thread::get_id()==modelThread); CHECK(!use64); ++readers;
        return reinterpret_cast<ARA::ARAAudioReaderHostRef>(ref);
    }
    bool readAudioSamples(ARA::ARAAudioReaderHostRef ref,ARA::ARASamplePosition start,ARA::ARASampleCount n,void* const dst[]) noexcept override {
        CHECK(std::this_thread::get_id()!=modelThread); ++reads;
        const auto& source=*reinterpret_cast<const TestAudioData*>(ref);
        for(int c=0;c<source.channels;++c) for(ARA::ARASampleCount i=0;i<n;++i) {
            const double time=double(start+i)/source.rate;
            static_cast<float*>(dst[c])[i]=time>=0 && time<1 ? float(.6*std::sin(2*3.141592653589793*source.hz*time)) : 0.f;
        }
        return true;
    }
    void destroyAudioReader(ARA::ARAAudioReaderHostRef) noexcept override { CHECK(std::this_thread::get_id()==modelThread); --readers; }
    ARA::ARASize getArchiveSize(ARA::ARAArchiveReaderHostRef) noexcept override { return 0; }
    bool readBytesFromArchive(ARA::ARAArchiveReaderHostRef,ARA::ARASize,ARA::ARASize,ARA::ARAByte[]) noexcept override { return false; }
    bool writeBytesToArchive(ARA::ARAArchiveWriterHostRef,ARA::ARASize,ARA::ARASize,const ARA::ARAByte[]) noexcept override { return true; }
    void notifyDocumentArchivingProgress(float) noexcept override {}
    void notifyDocumentUnarchivingProgress(float) noexcept override {}
    ARA::ARAPersistentID getDocumentArchiveID(ARA::ARAArchiveReaderHostRef) noexcept override { return "test"; }
};
bool wait(AnalyzedSource& source) {
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(source.snapshot().state==pg::AnalysisState::analyzing && std::chrono::steady_clock::now()<until) std::this_thread::yield();
    return source.snapshot().state==pg::AnalysisState::ready;
}
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    auto* factory=juce::ARADocumentControllerSpecialisation::createARAFactory<PitchGardenDocumentController>();
    ARA::ARAAssertFunction assertFunction=nullptr;
    ARA::ARAInterfaceConfiguration config{sizeof(config),ARA::kARAAPIGeneration_2_0_Final,&assertFunction};
    factory->initializeARAWithConfiguration(&config);
    Host host;
    ARA::Host::DocumentControllerHostInstance hostInstance(&host,&host);
    ARA::ARADocumentProperties document{sizeof(document),"Test document"};
    const auto* instance=factory->createDocumentControllerWithDocument(&hostInstance,&document);
    ARA::Host::DocumentController controller(instance);
    TestAudioData sourceData;
    controller.beginEditing();
    ARA::ARAMusicalContextProperties contextProperties{}; contextProperties.structSize=sizeof(contextProperties); contextProperties.name="Context";
    auto context=controller.createMusicalContext(reinterpret_cast<ARA::ARAMusicalContextHostRef>(&host),&contextProperties);
    ARA::ARARegionSequenceProperties sequenceProperties{}; sequenceProperties.structSize=sizeof(sequenceProperties); sequenceProperties.name="Track"; sequenceProperties.musicalContextRef=context;
    auto sequence=controller.createRegionSequence(reinterpret_cast<ARA::ARARegionSequenceHostRef>(&host),&sequenceProperties);
    ARA::ARAAudioSourceProperties props{}; props.structSize=sizeof(props); props.name="A4 source"; props.persistentID="test-source";
    props.sampleCount=48000; props.sampleRate=48000; props.channelCount=1;
    auto ref=controller.createAudioSource(reinterpret_cast<ARA::ARAAudioSourceHostRef>(&sourceData),&props);
    ARA::ARAAudioModificationProperties modProps{}; modProps.structSize=sizeof(modProps); modProps.persistentID="test-mod";
    auto mod=controller.createAudioModification(ref,reinterpret_cast<ARA::ARAAudioModificationHostRef>(&host),&modProps);
    ARA::ARAPlaybackRegionProperties regionProps{}; regionProps.structSize=sizeof(regionProps); regionProps.regionSequenceRef=sequence;
    regionProps.startInModificationTime=.2; regionProps.durationInModificationTime=.5;
    regionProps.startInPlaybackTime=12; regionProps.durationInPlaybackTime=.5;
    auto region=controller.createPlaybackRegion(mod,reinterpret_cast<ARA::ARAPlaybackRegionHostRef>(&host),&regionProps);
    controller.endEditing();
    controller.enableAudioSourceSamplesAccess(ref,true);
    auto* source=static_cast<AnalyzedSource*>(ARA::PlugIn::fromRef(ref));
    CHECK(wait(*source)); CHECK(!source->snapshot().result->notes.empty());
    CHECK(std::abs(source->snapshot().result->notes[0].continuousMidi-69)<.1);
    // Render actual ARA regions with nonzero source and timeline offsets.
    struct Lock final : ProcessingLockInterface { juce::ReadWriteLock rw; juce::ScopedTryReadLock getProcessingLock() override { return juce::ScopedTryReadLock(rw); } } lock;
    {
        PitchGardenPlayback renderer(source->getDocumentController(),lock);
        renderer.addPlaybackRegion(region);
        renderer.prepareToPlay(48000,512,2,juce::AudioProcessor::singlePrecision,juce::ARARenderer::AlwaysNonRealtime::yes);
        juce::AudioBuffer<float> output(2,512); juce::AudioPlayHead::PositionInfo pos;
        pos.setIsPlaying(true); pos.setTimeInSamples(12*48000);
        renderer.processBlock(output,juce::AudioProcessor::Realtime::no,pos);
        for(int c=0;c<2;++c) for(int i=0;i<512;++i) {
            const auto expected=float(.6*std::sin(2*3.141592653589793*440*double(9600+i)/48000));
            CHECK(std::abs(output.getSample(c,i)-expected)<1e-6);
        }
        // Region move/trim changes mapping without changing source results.
        const auto result=source->snapshot().result;
        controller.beginEditing(); regionProps.startInPlaybackTime=20; regionProps.startInModificationTime=.4;
        regionProps.durationInPlaybackTime=regionProps.durationInModificationTime=.25;
        controller.updatePlaybackRegionProperties(region,&regionProps); controller.endEditing();
        CHECK(source->snapshot().result==result);
        pos.setTimeInSamples(20*48000); renderer.processBlock(output,juce::AudioProcessor::Realtime::no,pos);
        CHECK(std::abs(output.getSample(0,10)-float(.6*std::sin(2*3.141592653589793*440*double(19210)/48000)))<1e-6);
        renderer.releaseResources(); renderer.removePlaybackRegion(region);
    }
    controller.enableAudioSourceSamplesAccess(ref,false);
    CHECK(!source->snapshot().result); CHECK(host.readers.load()==0);
    // Replace source with stereo 96 kHz material and reanalyze independently.
    controller.beginEditing(); sourceData={96000,2,220}; props.sampleRate=96000; props.sampleCount=96000; props.channelCount=2;
    controller.updateAudioSourceProperties(ref,&props); controller.endEditing();
    controller.enableAudioSourceSamplesAccess(ref,true); CHECK(wait(*source));
    CHECK(std::abs(source->snapshot().result->notes[0].continuousMidi-57)<.1);
    // Render screenshot from the same immutable model without a live host GUI.
    PitchRegionDisplay display{"Stereo test",{12,.2,.5,.5},source->snapshot(),true};
    PitchEditor editor([&] { return std::vector<PitchRegionDisplay>{display}; }); editor.setSize(1000,560);
    auto image=editor.createComponentSnapshot(editor.getLocalBounds());
    juce::File outputImage("/tmp/pitchgarden-piano-roll.png"); outputImage.deleteFile();
    if(auto stream=outputImage.createOutputStream()) juce::PNGImageFormat().writeImageToStream(image,*stream);
    controller.enableAudioSourceSamplesAccess(ref,false);
    controller.beginEditing(); controller.destroyPlaybackRegion(region); controller.destroyAudioModification(mod);
    controller.destroyAudioSource(ref); controller.destroyRegionSequence(sequence); controller.destroyMusicalContext(context); controller.endEditing();
    controller.destroyDocumentController(); CHECK(host.readers.load()==0); CHECK(host.reads.load()>0);
    factory->uninitializeARA();
    std::cout << "ARA host sample acquisition, analysis, unity render, offsets, moves, trims, stereo 96k replacement and destruction passed\n";
}
