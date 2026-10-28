#include "PitchGardenARA.h"
#include <ARA_Library/Dispatch/ARAHostDispatch.h>
#include <ARA_Library/Dispatch/ARAHostDispatch.cpp>
#include <chrono>
#include <iostream>
#include <thread>
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
struct TestAudioData { double rate=48000; int channels=1; double hz=440; double duration=10; };
class Host final : public ARA::Host::AudioAccessControllerInterface,public ARA::Host::ArchivingControllerInterface {
public:
    const std::thread::id modelThread=std::this_thread::get_id();
    std::atomic<int> readers{0},reads{0};
    std::atomic<bool> blockReads{false},readBlocked{false};
    ARA::ARAAudioReaderHostRef createAudioReaderForSource(ARA::ARAAudioSourceHostRef ref,bool use64) noexcept override {
        CHECK(std::this_thread::get_id()==modelThread); CHECK(!use64); ++readers;
        return reinterpret_cast<ARA::ARAAudioReaderHostRef>(ref);
    }
    bool readAudioSamples(ARA::ARAAudioReaderHostRef ref,ARA::ARASamplePosition start,ARA::ARASampleCount n,void* const dst[]) noexcept override {
        CHECK(std::this_thread::get_id()!=modelThread); ++reads;
        if(blockReads.load()) { readBlocked.store(true); while(blockReads.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        const auto& source=*reinterpret_cast<const TestAudioData*>(ref);
        for(int c=0;c<source.channels;++c) for(ARA::ARASampleCount i=0;i<n;++i) {
            const double time=double(start+i)/source.rate;
            static_cast<float*>(dst[c])[i]=time>=0 && time<source.duration ? float(.6*std::sin(2*3.141592653589793*source.hz*time)) : 0.f;
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
    { // Non-ARA fallback, including hosts without a playhead, preserves dry input.
        PitchGardenProcessor processor;
        processor.prepareToPlay(48000,512);
        juce::AudioBuffer<float> dry(2,512); juce::MidiBuffer midi;
        for(int c=0;c<2;++c) for(int i=0;i<512;++i) dry.setSample(c,i,.123f);
        processor.processBlock(dry,midi);
        for(int c=0;c<2;++c) for(int i=0;i<512;++i) CHECK(dry.getSample(c,i)==.123f);
        processor.releaseResources();
    }
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
    props.sampleCount=480000; props.sampleRate=48000; props.channelCount=1;
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
    // Separate ARA documents must never publish each other's results.
    {
        Host otherHost; ARA::Host::DocumentControllerHostInstance otherHostInstance(&otherHost,&otherHost);
        const auto* otherInstance=factory->createDocumentControllerWithDocument(&otherHostInstance,&document);
        ARA::Host::DocumentController otherController(otherInstance);
        TestAudioData otherData{48000,1,660,1}; auto otherProps=props;
        otherProps.sampleCount=48000; otherProps.persistentID="other-source";
        otherController.beginEditing();
        const auto otherRef=otherController.createAudioSource(reinterpret_cast<ARA::ARAAudioSourceHostRef>(&otherData),&otherProps);
        otherController.endEditing(); otherController.enableAudioSourceSamplesAccess(otherRef,true);
        auto* otherSource=static_cast<AnalyzedSource*>(ARA::PlugIn::fromRef(otherRef)); CHECK(wait(*otherSource));
        CHECK(otherSource->snapshot().result!=source->snapshot().result);
        CHECK(std::abs(otherSource->snapshot().result->notes[0].continuousMidi-pg::frequencyToMidi(660))<.1);
        CHECK(std::abs(source->snapshot().result->notes[0].continuousMidi-69)<.1);
        otherController.enableAudioSourceSamplesAccess(otherRef,false);
        otherController.beginEditing(); otherController.destroyAudioSource(otherRef); otherController.endEditing();
        otherController.destroyDocumentController(); CHECK(otherHost.readers.load()==0);
    }
    // Render actual ARA regions with nonzero source and timeline offsets.
    struct Lock final : ProcessingLockInterface { pg::RenderGate gate; pg::RenderGate::ReadToken getProcessingLock() override { return gate.tryRead(); } } lock;
    {
        PitchGardenPlayback renderer(source->getDocumentController(),lock);
        renderer.addPlaybackRegion(region);
        renderer.prepareToPlay(48000,512,2,juce::AudioProcessor::singlePrecision,juce::ARARenderer::AlwaysNonRealtime::yes);
        juce::AudioBuffer<float> output(2,512); juce::AudioPlayHead::PositionInfo pos;
        pos.setIsPlaying(true); pos.setTimeInSamples(12*48000);
        renderer.processBlock(output,juce::AudioProcessor::Realtime::no,pos);
        for(int c=0;c<2;++c) for(int i=0;i<512;++i) {
            const double sourceTime=double(9600+i)/48000;
            const auto expected=float(.6*std::sin(2*3.141592653589793*440*sourceTime));
            CHECK(output.getSample(c,i)==expected);
        }
        // Region move/trim changes mapping without changing source results.
        const auto result=source->snapshot().result;
        controller.beginEditing(); regionProps.startInPlaybackTime=20; regionProps.startInModificationTime=.4;
        regionProps.durationInPlaybackTime=regionProps.durationInModificationTime=.25;
        controller.updatePlaybackRegionProperties(region,&regionProps); controller.endEditing();
        CHECK(source->snapshot().result==result);
        pos.setTimeInSamples(20*48000); renderer.processBlock(output,juce::AudioProcessor::Realtime::no,pos);
        CHECK(std::abs(output.getSample(0,10)-float(.6*std::sin(2*3.141592653589793*440*double(19210)/48000)))<1e-6);
        // Block host I/O during a distant seek. Realtime rendering must return
        // promptly and retain every host input sample, even while I/O is blocked.
        host.blockReads.store(true); host.readBlocked.store(false);
        controller.beginEditing(); regionProps.startInModificationTime=5;
        controller.updatePlaybackRegionProperties(region,&regionProps); controller.endEditing();
        auto realtimeMiss=[&] {
            for(int c=0;c<2;++c) for(int i=0;i<512;++i) output.setSample(c,i,.123f);
            const auto began=std::chrono::steady_clock::now();
            renderer.processBlock(output,juce::AudioProcessor::Realtime::yes,pos);
            CHECK(std::chrono::steady_clock::now()-began<std::chrono::milliseconds(50));
            for(int c=0;c<2;++c) for(int i=0;i<512;++i) CHECK(output.getSample(c,i)==.123f);
        };
        realtimeMiss();
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!host.readBlocked.load() && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();
        CHECK(host.readBlocked.load()); realtimeMiss(); host.blockReads.store(false);
        renderer.processBlock(output,juce::AudioProcessor::Realtime::no,pos);
        CHECK(std::abs(output.getSample(0,10)-float(.6*std::sin(2*3.141592653589793*440*double(240010)/48000)))<1e-6);
        renderer.releaseResources(); renderer.removePlaybackRegion(region);
    }
    controller.enableAudioSourceSamplesAccess(ref,false);
    CHECK(!source->snapshot().result); CHECK(host.readers.load()==0);
    // Replace source with stereo 96 kHz material and reanalyze independently.
    controller.beginEditing(); sourceData={96000,2,220,1}; props.sampleRate=96000; props.sampleCount=96000; props.channelCount=2;
    controller.updateAudioSourceProperties(ref,&props); controller.endEditing();
    controller.enableAudioSourceSamplesAccess(ref,true); CHECK(wait(*source));
    CHECK(std::abs(source->snapshot().result->notes[0].continuousMidi-57)<.1);
    {
        PitchGardenPlayback renderer(source->getDocumentController(),lock);
        renderer.addPlaybackRegion(region);
        renderer.prepareToPlay(48000,512,2,juce::AudioProcessor::singlePrecision,juce::ARARenderer::AlwaysNonRealtime::yes);
        juce::AudioBuffer<float> converted(2,512); juce::AudioPlayHead::PositionInfo pos;
        controller.beginEditing(); regionProps.startInModificationTime=.2;
        controller.updatePlaybackRegionProperties(region,&regionProps); controller.endEditing();
        pos.setIsPlaying(true); pos.setTimeInSamples(20*48000);
        renderer.processBlock(converted,juce::AudioProcessor::Realtime::no,pos);
        for(int c=0;c<2;++c) for(int i=0;i<512;++i)
            CHECK(std::abs(converted.getSample(c,i)-float(.6*std::sin(2*3.141592653589793*220*double(19200+2*i)/96000)))<1e-6);
        renderer.releaseResources(); renderer.removePlaybackRegion(region);
    }
    // Render screenshot from the same immutable model without a live host GUI.
    PitchRegionDisplay display{"Stereo test",{12,.2,.5,.5},source->snapshot(),true};
    PitchEditor editor([&] { return std::vector<PitchRegionDisplay>{display}; }); editor.setSize(1000,560);
    auto image=editor.createComponentSnapshot(editor.getLocalBounds());
    auto outputImage=juce::File::getCurrentWorkingDirectory().getChildFile("pitchgarden-piano-roll.png"); outputImage.deleteFile();
    if(auto stream=outputImage.createOutputStream()) juce::PNGImageFormat().writeImageToStream(image,*stream);
    controller.enableAudioSourceSamplesAccess(ref,false);
    controller.beginEditing(); controller.destroyPlaybackRegion(region); controller.destroyAudioModification(mod);
    controller.destroyAudioSource(ref); controller.destroyRegionSequence(sequence); controller.destroyMusicalContext(context); controller.endEditing();
    controller.destroyDocumentController(); CHECK(host.readers.load()==0); CHECK(host.reads.load()>0);
    factory->uninitializeARA();
    std::cout << "ARA host sample acquisition, analysis, unity render, offsets, moves, trims, stereo 96k replacement and destruction passed\n";
}
