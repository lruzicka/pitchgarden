#include "PitchGardenARA.h"
#include <ARA_Library/Dispatch/ARAHostDispatch.h>
#include <ARA_Library/Dispatch/ARAHostDispatch.cpp>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
struct TestAudioData { double rate=48000; int channels=1; double hz=440; double duration=10; bool invertedRight=false; double secondAmplitude=0; };
class Host final : public ARA::Host::AudioAccessControllerInterface,public ARA::Host::ArchivingControllerInterface,public ARA::Host::PlaybackControllerInterface {
public:
    int startRequests=0,stopRequests=0,positionRequests=0,cycleRequests=0;
    void requestStartPlayback() noexcept override { CHECK(std::this_thread::get_id()==modelThread); ++startRequests; }
    void requestStopPlayback() noexcept override { CHECK(std::this_thread::get_id()==modelThread); ++stopRequests; }
    void requestSetPlaybackPosition(ARA::ARATimePosition) noexcept override { ++positionRequests; }
    void requestSetCycleRange(ARA::ARATimePosition,ARA::ARATimeDuration) noexcept override { ++cycleRequests; }
    void requestEnableCycle(bool) noexcept override { ++cycleRequests; }
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
            static_cast<float*>(dst[c])[i]=time>=0 && time<source.duration ? float((c==1 && source.invertedRight ? -.6 : .6)*std::sin(2*3.141592653589793*source.hz*time)+source.secondAmplitude*std::sin(4*3.141592653589793*source.hz*time)) : 0.f;
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
class TestPlayHead final : public juce::AudioPlayHead {
public:
    juce::AudioPlayHead::PositionInfo position;
    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override { return position; }
};
bool wait(AnalyzedSource& source) {
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(source.snapshot().state==pg::AnalysisState::analyzing && std::chrono::steady_clock::now()<until) std::this_thread::yield();
    return source.snapshot().state==pg::AnalysisState::ready;
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    { // Non-ARA fallback, including hosts without a playhead, preserves dry input.
        PitchGardenProcessor processor;
        processor.prepareToPlay(48000,512);
        juce::AudioBuffer<float> dry(2,512); juce::MidiBuffer midi;
        for(int c=0;c<2;++c) for(int i=0;i<512;++i) dry.setSample(c,i,.123f);
        processor.processBlock(dry,midi);
        for(int c=0;c<2;++c) for(int i=0;i<512;++i) CHECK(dry.getSample(c,i)==.123f);
        CHECK(!processor.transport.read().available);
        TestPlayHead playhead; processor.setPlayHead(&playhead);
        for(double seconds : {12.,13.,3.,4.}) {
            playhead.position.setTimeInSeconds(seconds); playhead.position.setIsPlaying(true); playhead.position.setIsLooping(true);
            processor.processBlock(dry,midi); const auto snapshot=processor.transport.read();
            CHECK(snapshot.available && snapshot.playing && snapshot.looping && snapshot.projectSeconds==seconds);
        }
        playhead.position.setTimeInSeconds(2); playhead.position.setIsPlaying(false);
        processor.processBlock(dry,midi); CHECK(!processor.transport.read().playing && processor.transport.read().projectSeconds==2);
        processor.setPlayHead(nullptr); processor.releaseResources();
    }
    auto* factory=juce::ARADocumentControllerSpecialisation::createARAFactory<PitchGardenDocumentController>();
    ARA::ARAAssertFunction assertFunction=nullptr;
    ARA::ARAInterfaceConfiguration config{sizeof(config),ARA::kARAAPIGeneration_2_0_Final,&assertFunction};
    factory->initializeARAWithConfiguration(&config);
    Host host;
    ARA::Host::DocumentControllerHostInstance hostInstance(&host,&host,nullptr,nullptr,&host);
    ARA::ARADocumentProperties document{sizeof(document),"Test document"};
    const auto* instance=factory->createDocumentControllerWithDocument(&hostInstance,&document);
    ARA::Host::DocumentController controller(instance);
    {
        PitchGardenProcessor processor;
        processor.getARAClientExtensions()->bindToARA(instance->documentControllerRef,
            ARA::kARAPlaybackRendererRole|ARA::kARAEditorRendererRole|ARA::kARAEditorViewRole,ARA::kARAEditorViewRole);
        TestPlayHead playhead; playhead.position.setTimeInSeconds(12); processor.setPlayHead(&playhead);
        processor.prepareToPlay(48000,512);
        juce::AudioBuffer<float> audio(2,512); audio.clear(); juce::MidiBuffer midi; processor.processBlock(audio,midi);
        {
            PitchGardenProcessorEditor editor(processor);
            CHECK(!editor.getWantsKeyboardFocus());
            CHECK(editor.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey))); CHECK(host.startRequests==1);
            CHECK(editor.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey))); CHECK(host.startRequests==1);
            editor.keyStateChanged(false);
            playhead.position.setIsPlaying(true); processor.processBlock(audio,midi);
            CHECK(editor.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey))); CHECK(host.stopRequests==1);
            CHECK(!editor.keyPressed(juce::KeyPress('x')));
            CHECK(host.positionRequests==0 && host.cycleRequests==0);
            // Opt-in native peer dispatch test; creates only our own test window.
            if(argc>1 && std::string(argv[1])=="--native-keys") {
                juce::Component wrapper;
                wrapper.setSize(1000,560); wrapper.addAndMakeVisible(editor);
                wrapper.addToDesktop(0); wrapper.setVisible(true);
                juce::Component::unfocusAllComponents();
                editor.keyStateChanged(false);
                const auto stops=host.stopRequests;
                CHECK(wrapper.getPeer()!=nullptr);
                CHECK(wrapper.getPeer()->handleKeyPress(juce::KeyPress(juce::KeyPress::spaceKey)));
                CHECK(host.stopRequests==stops+1);
                CHECK(!wrapper.getPeer()->handleKeyPress(juce::KeyPress('x')));
                // A lost key-up must not leave the fallback latched. Peer
                // dispatch does not alter physical key state; this exercises
                // the watchdog's stale-latch branch, not real OS focus loss.
                wrapper.getPeer()->handleFocusLoss();
                juce::Timer::callAfterDelay(100,[] { juce::MessageManager::getInstance()->stopDispatchLoop(); });
                juce::MessageManager::getInstance()->runDispatchLoop();
                CHECK(wrapper.getPeer()->handleKeyPress(juce::KeyPress(juce::KeyPress::spaceKey)));
                CHECK(host.stopRequests==stops+2);
                wrapper.removeFromDesktop();
            }
        }
        processor.releaseResources(); processor.setPlayHead(nullptr);
    }
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
        // Cancelling only the fundamental used to leave the common second
        // harmonic at 392 Hz. Validate the real ARA bridge at 44.1 kHz too.
        otherData={44100,2,196,1,true,.65};
        otherProps.sampleRate=44100; otherProps.channelCount=2; otherProps.sampleCount=44100;
        otherController.beginEditing(); otherController.updateAudioSourceProperties(otherRef,&otherProps); otherController.endEditing();
        otherController.enableAudioSourceSamplesAccess(otherRef,true); CHECK(wait(*otherSource));
        CHECK(!otherSource->snapshot().result->notes.empty());
        CHECK(std::abs(otherSource->snapshot().result->notes[0].continuousMidi-55)<.1);
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
        // JUCE permits a host to supply project seconds without sample position.
        juce::AudioPlayHead::PositionInfo secondsOnly; secondsOnly.setIsPlaying(true); secondsOnly.setTimeInSeconds(12);
        renderer.processBlock(output,juce::AudioProcessor::Realtime::no,secondsOnly);
        CHECK(std::abs(output.getSample(0,10)-float(.6*std::sin(2*3.141592653589793*440*double(9610)/48000)))<1e-6);
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
    {
        PitchGardenProcessor previewProcessor;
        const auto roles=ARA::kARAEditorViewRole|ARA::kARAEditorRendererRole;
        previewProcessor.getARAClientExtensions()->bindToARA(instance->documentControllerRef,ARA::kARAPlaybackRendererRole|ARA::kARAEditorRendererRole|ARA::kARAEditorViewRole,roles);
        auto* preview=previewProcessor.auditionRenderer(); CHECK(preview!=nullptr);
        auto* modelRegion=static_cast<PitchGardenRegion*>(ARA::PlugIn::fromRef(region));
        const auto map=PitchGardenPlayback::mapping(*modelRegion);
        const NoteAuditionRequest request{modelRegion->identity,source->revision.load(),map,{map.sourceStart,map.sourceStart+.1}};
        CHECK(!preview->submit(request)); // host did not assign region yet
        preview->addPlaybackRegion(region);
        previewProcessor.prepareToPlay(48000,512);
        TestPlayHead playhead; playhead.position.setTimeInSeconds(20); previewProcessor.setPlayHead(&playhead);
        juce::AudioBuffer<float> audio(2,512),warm(2,4096); juce::MidiBuffer midi;
        CHECK(source->playback.read(warm,4096,240000,5000));
        const auto fill=[&] { for(int c=0;c<2;++c) for(int i=0;i<512;++i) audio.setSample(c,i,.123f); };
        const auto unchanged=[&] { for(int c=0;c<2;++c) for(int i=0;i<512;++i) CHECK(audio.getSample(c,i)==.123f); };
        const int starts=host.startRequests,stops=host.stopRequests,positions=host.positionRequests,cycles=host.cycleRequests;
        CHECK(preview->submit(request));
        juce::AudioPlayHead::PositionInfo stopped;
        fill(); preview->processBlock(audio,juce::AudioProcessor::Realtime::no,stopped); unchanged();
        CHECK(preview->getStatus()==NoteAuditionStatus::queued); // never preview during export
        fill(); previewProcessor.processBlock(audio,midi);
        CHECK(preview->getStatus()==NoteAuditionStatus::playing);
        const double sampleTime=5+300.0/48000;
        CHECK(std::abs(audio.getSample(0,300)-(.123f+float(.6*std::sin(2*3.141592653589793*440*sampleTime))))<1e-6);
        CHECK(audio.getSample(0,0)==.123f); // preview-only attack ramp
        CHECK(previewProcessor.transport.read().projectSeconds==20);
        for(int i=0;i<10;++i) { fill(); previewProcessor.processBlock(audio,midi); }
        CHECK(preview->getStatus()==NoteAuditionStatus::finished);
        {
            // Suspend an audio callback after it observes the queued request,
            // then stop from the model thread before allowing it to continue.
            struct CoordinatedLock final : ProcessingLockInterface {
                pg::RenderGate gate; std::promise<void> entered,release;
                std::shared_future<void> proceed=release.get_future().share();
                pg::RenderGate::ReadToken getProcessingLock() override { entered.set_value(); proceed.wait(); return gate.tryRead(); }
            } coordinated;
            auto entered=coordinated.entered.get_future();
            PitchGardenAudition concurrent(source->getDocumentController(),coordinated);
            concurrent.addPlaybackRegion(region);
            concurrent.prepareToPlay(48000,512,2,juce::AudioProcessor::singlePrecision,juce::ARARenderer::AlwaysNonRealtime::no);
            CHECK(concurrent.submit(request)); fill();
            std::thread callback([&] { concurrent.processBlock(audio,juce::AudioProcessor::Realtime::yes,stopped); });
            CHECK(entered.wait_for(std::chrono::seconds(2))==std::future_status::ready);
            std::promise<void> stopping; auto stopStarted=stopping.get_future();
            auto stop=std::async(std::launch::async,[&] { stopping.set_value(); concurrent.stop(); });
            stopStarted.wait();
            CHECK(stop.wait_for(std::chrono::milliseconds(10))==std::future_status::timeout);
            coordinated.release.set_value(); callback.join(); stop.get();
            CHECK(concurrent.getStatus()==NoteAuditionStatus::idle);
            fill(); concurrent.processBlock(audio,juce::AudioProcessor::Realtime::yes,stopped); unchanged();
            concurrent.releaseResources(); CHECK(!concurrent.submit(request)); concurrent.removePlaybackRegion(region);
        }
        preview->stop(); fill(); previewProcessor.processBlock(audio,midi); unchanged();
        CHECK(preview->submit(request)); playhead.position.setIsPlaying(true);
        fill(); previewProcessor.processBlock(audio,midi); unchanged(); CHECK(preview->getStatus()==NoteAuditionStatus::hostPlaying);
        playhead.position.setIsPlaying(false);
        CHECK(preview->submit(request));
        controller.beginEditing(); regionProps.startInPlaybackTime=21;
        controller.updatePlaybackRegionProperties(region,&regionProps); controller.endEditing();
        fill(); previewProcessor.processBlock(audio,midi); unchanged(); CHECK(preview->getStatus()==NoteAuditionStatus::invalid);
        controller.beginEditing(); regionProps.startInPlaybackTime=20;
        controller.updatePlaybackRegionProperties(region,&regionProps); controller.endEditing();
        CHECK(preview->submit(request));
        preview->removePlaybackRegion(region); preview->addPlaybackRegion(region);
        fill(); previewProcessor.processBlock(audio,midi); unchanged();
        CHECK(preview->getStatus()==NoteAuditionStatus::invalid);
        CHECK(preview->submit(request)); controller.enableAudioSourceSamplesAccess(ref,false);
        fill(); previewProcessor.processBlock(audio,midi); unchanged(); CHECK(preview->getStatus()==NoteAuditionStatus::invalid);
        CHECK(host.startRequests==starts && host.stopRequests==stops && host.positionRequests==positions && host.cycleRequests==cycles);
        preview->removePlaybackRegion(region); CHECK(!preview->submit(request));
        previewProcessor.releaseResources(); previewProcessor.setPlayHead(nullptr);
    }
    controller.enableAudioSourceSamplesAccess(ref,false);
    CHECK(!source->snapshot().result); CHECK(host.readers.load()==0);
    // Replace source with stereo 96 kHz material and reanalyze independently.
    controller.beginEditing(); sourceData={96000,2,220,1,true}; props.sampleRate=96000; props.sampleCount=96000; props.channelCount=2;
    controller.updateAudioSourceProperties(ref,&props); controller.endEditing();
    controller.enableAudioSourceSamplesAccess(ref,true); CHECK(wait(*source));
    CHECK(!source->snapshot().result->notes.empty());
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
            CHECK(std::abs(converted.getSample(c,i)-float((c==1 ? -.6 : .6)*std::sin(2*3.141592653589793*220*double(19200+2*i)/96000)))<1e-6);
        renderer.releaseResources(); renderer.removePlaybackRegion(region);
    }
    // Render screenshot from the same immutable model without a live host GUI.
    PitchRegionDisplay display{"Stereo test",{12,.2,.5,.5},source->snapshot(),true};
    PitchEditor editor([&] { return std::vector<PitchRegionDisplay>{display}; },[] { return pg::TransportSnapshot{12.2,true,true,true}; }); editor.setSize(1000,560);
    juce::ToggleButton* follow=nullptr; juce::ToggleButton* diagnostics=nullptr; juce::TextButton* fit=nullptr;
    for(auto* child:editor.getChildren()) if(auto* button=dynamic_cast<juce::Button*>(child)) {
        CHECK(!button->getWantsKeyboardFocus());
        if(button->getButtonText()=="Diagnostics") diagnostics=dynamic_cast<juce::ToggleButton*>(button);
        if(button->getButtonText()=="Follow") follow=dynamic_cast<juce::ToggleButton*>(button);
        if(button->getButtonText()=="Fit") fit=dynamic_cast<juce::TextButton*>(button);
    }
    CHECK(follow!=nullptr && fit!=nullptr); CHECK(!follow->getToggleState());
    follow->setToggleState(true,juce::dontSendNotification);
    const juce::Point<float> mousePosition{500,300};
    const juce::MouseEvent wheelEvent(juce::Desktop::getInstance().getMainMouseSource(),mousePosition,{},1,0,0,0,0,
        &editor,&editor,{},mousePosition,{},1,false);
    juce::MouseWheelDetails wheel; wheel.deltaY=.1f; editor.mouseWheelMove(wheelEvent,wheel);
    CHECK(!follow->getToggleState()); fit->onClick();
    CHECK(diagnostics!=nullptr); diagnostics->setToggleState(true,juce::dontSendNotification); editor.mouseMove(wheelEvent);
    auto image=editor.createComponentSnapshot(editor.getLocalBounds());
    auto outputImage=juce::File::getCurrentWorkingDirectory().getChildFile("pitchgarden-piano-roll.png"); outputImage.deleteFile();
    if(auto stream=outputImage.createOutputStream()) juce::PNGImageFormat().writeImageToStream(image,*stream);
    int heldStarts=0,heldStops=0;
    {
        const auto original=display.analysis.result;
        const auto noteCount=original->notes.size(); const auto originalPitch=original->notes[0].continuousMidi;
        PitchEditor held([&] { return std::vector<PitchRegionDisplay>{display}; },{},
            {[] { return true; },[&](const auto& r,const auto& n) {
                CHECK(r.analysis.result==original && n.midiNote==57);
                const auto interval=pg::auditionInterval(r.mapping,n.start,n.end);
                CHECK(interval && interval->start==.2 && interval->end<=.7);
                ++heldStarts; return true;
            },[&] { ++heldStops; },{}});
        held.setSize(1000,560);
        const juce::Point<float> point{500,426};
        const juce::MouseEvent press(juce::Desktop::getInstance().getMainMouseSource(),point,
            juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&held,&held,{},point,{},1,false);
        const auto outside=press.withNewPosition(juce::Point<float>{-20,-20});
        held.mouseDown(press); CHECK(heldStarts==1 && heldStops==0);
        held.mouseUp(outside); CHECK(heldStops==1);
        held.mouseUp(outside); CHECK(heldStops==1); // idempotent cancellation
        held.mouseDown(press); held.mouseDown(press); CHECK(heldStarts==3 && heldStops==2);
        held.mouseUp(outside); CHECK(heldStops==3);
        held.mouseDown(press); held.setEnabled(false); CHECK(heldStops==4);
        held.setEnabled(true); held.mouseDown(press); CHECK(heldStarts==5);
        CHECK(original->notes.size()==noteCount && original->notes[0].continuousMidi==originalPitch);
        // Closing with the button still held must stop the final voice.
    }
    CHECK(heldStarts==5 && heldStops==5);
    controller.enableAudioSourceSamplesAccess(ref,false);
    controller.beginEditing(); controller.destroyPlaybackRegion(region); controller.destroyAudioModification(mod);
    controller.destroyAudioSource(ref); controller.destroyRegionSequence(sequence); controller.destroyMusicalContext(context); controller.endEditing();
    controller.destroyDocumentController(); CHECK(host.readers.load()==0); CHECK(host.reads.load()>0);
    factory->uninitializeARA();
    std::cout << "ARA host sample acquisition, analysis, unity render, offsets, moves, trims, stereo 96k replacement and destruction passed\n";
}
