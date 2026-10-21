/*
  ==============================================================================

   This file is part of the JUCE framework examples.
   Copyright (c) Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   to use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
   REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
   INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
   OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
   PERFORMANCE OF THIS SOFTWARE.

  ==============================================================================
*/

#pragma once
#include "PitchGardenPlayback.h"
#include "TransportState.h"
#include "PitchGardenAudition.h"

class PitchGardenDocumentController final : public ARADocumentControllerSpecialisation,
                                                            private ProcessingLockInterface
{
public:
    using ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;


protected:
    ARAAudioSource* doCreateAudioSource(ARADocument* document, ARA::ARAAudioSourceHostRef hostRef) noexcept override {
        return new AnalyzedSource(document,hostRef);
    }
    void willBeginEditing (ARADocument*) override
    {
        processBlockLock.beginWrite();
    }

    void didEndEditing (ARADocument*) override
    {
        processBlockLock.endWrite();
        for(auto* source : getDocumentController()->getDocument<ARADocument>()->getAudioSources<AnalyzedSource>())
            source->ensureAnalysis();
    }

    ARAAudioModification* doCreateAudioModification (ARAAudioSource* audioSource,
                                                     ARA::ARAAudioModificationHostRef hostRef,
                                                     const ARAAudioModification* optionalModificationToClone) noexcept override
    {
        return new ARAAudioModification(audioSource,hostRef,optionalModificationToClone);
    }

    ARAPlaybackRegion* doCreatePlaybackRegion(ARAAudioModification* modification,ARA::ARAPlaybackRegionHostRef hostRef) noexcept override {
        return new PitchGardenRegion(modification,hostRef);
    }
    ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override
    {
        return new PitchGardenPlayback (getDocumentController(), *this);
    }

    ARAEditorRenderer* doCreateEditorRenderer() noexcept override
    {
        return new PitchGardenAudition(getDocumentController(),*this);
    }

    bool doRestoreObjectsFromStream (ARAInputStream& input,
                                     const ARARestoreObjectsFilter* filter) noexcept override
    {
        // Start reading data from the archive, starting with the number of audio modifications in the archive
        const auto numAudioModifications = input.readInt64();

        // Loop over stored audio modification data
        for (int64 i = 0; i < numAudioModifications; ++i)
        {
            const auto progressVal = (float) i / (float) numAudioModifications;
            getDocumentController()->getHostArchivingController()->notifyDocumentUnarchivingProgress (progressVal);

            // Read audio modification persistent ID and analysis result from archive
            const String persistentID = input.readString();
            input.readBool(); // consume legacy demo dim state; 0.2 always plays at unity
            ignoreUnused(persistentID, filter);

        }

        getDocumentController()->getHostArchivingController()->notifyDocumentUnarchivingProgress (1.0f);

        return ! input.failed();
    }

    bool doStoreObjectsToStream (ARAOutputStream& output, const ARAStoreObjectsFilter* filter) noexcept override
    {
        // This example implementation only deals with audio modification states
        const auto& audioModificationsToPersist { filter->getAudioModificationsToStore<ARAAudioModification>() };

        const auto reportProgress = [archivingController = getDocumentController()->getHostArchivingController()] (float p)
        {
            archivingController->notifyDocumentArchivingProgress (p);
        };

        const ScopeGuard scope { [&reportProgress] { reportProgress (1.0f); } };

        // Write the number of audio modifications we are persisting
        const auto numAudioModifications = audioModificationsToPersist.size();

        if (! output.writeInt64 ((int64) numAudioModifications))
            return false;

        // For each audio modification to persist, persist its ID followed by whether it's dimmed
        for (size_t i = 0; i < numAudioModifications; ++i)
        {
            // Write persistent ID and dim state
            if (! output.writeString (audioModificationsToPersist[i]->getPersistentID()))
                return false;

            if (! output.writeBool (false))
                return false;

            const auto progressVal = (float) i / (float) numAudioModifications;
            reportProgress (progressVal);
        }

        return true;
    }

private:
    pg::RenderGate::ReadToken getProcessingLock() override
    {
        return processBlockLock.tryRead();
    }

    pg::RenderGate processBlockLock;
};


//==============================================================================
class PitchGardenProcessorBase : public AudioProcessor,
                                        private AudioProcessorARAExtension
{
public:
    //==============================================================================
    PitchGardenProcessorBase()
        : AudioProcessor (getBusesProperties())
    {}

    ~PitchGardenProcessorBase() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        transport.publish({});
        processingRate=sampleRate;
        prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
    }

    void releaseResources() override
    {
        transport.publish({});
        releaseResourcesForARA();
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
            && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
            return false;

        return true;
    }

    void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override
    {
        ignoreUnused (midiMessages);

        ScopedNoDenormals noDenormals;

        auto* audioPlayHead = getPlayHead();
        const auto info=audioPlayHead != nullptr ? audioPlayHead->getPosition() : Optional<AudioPlayHead::PositionInfo>{};
        pg::TransportSnapshot state;
        if(info.hasValue()) {
            const auto seconds=info->getTimeInSeconds(); const auto samples=info->getTimeInSamples();
            state=pg::mapTransport(info->getIsPlaying(),info->getIsLooping(),
                seconds.hasValue() ? std::optional<double>(seconds.orFallback(0)) : std::nullopt,
                samples.hasValue() ? std::optional<std::int64_t>(samples.orFallback(0)) : std::nullopt,processingRate);
        }
        transport.publish(state);

        if (! processBlockForARA (buffer, isRealtime(), info.orFallback(AudioPlayHead::PositionInfo{})))
            processBlockBypassed (buffer, midiMessages);
    }

    using AudioProcessor::processBlock;

    //==============================================================================
    const String getName() const override                             { return JucePlugin_Name; }
    bool acceptsMidi() const override                                 { return false; }
    bool producesMidi() const override                                { return false; }

    double getTailLengthSeconds() const override
    {
        double tail;
        if (getTailLengthSecondsForARA (tail))
            return tail;

        return 0.0;
    }

    //==============================================================================
    int getNumPrograms() override                                     { return 0; }
    int getCurrentProgram() override                                  { return 0; }
    void setCurrentProgram (int) override                             {}
    const String getProgramName (int) override                        { return "None"; }
    void changeProgramName (int, const String&) override              {}

    //==============================================================================
    void getStateInformation (MemoryBlock&) override                  {}
    void setStateInformation (const void*, int) override              {}

    AudioProcessorARAExtension* getARAClientExtensions() override { return this; }

    pg::TransportState transport;
    PitchGardenAudition* auditionRenderer() const { return getEditorRenderer<PitchGardenAudition>(); }

private:
    double processingRate=0;
    //==============================================================================
    static BusesProperties getBusesProperties()
    {
        return BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
            .withOutput ("Output", AudioChannelSet::stereo(), true);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchGardenProcessorBase)
};

//==============================================================================
#include "PitchEditor.h"

class PitchGardenProcessorEditor final : public AudioProcessorEditor,
                                        private AudioProcessorEditorARAExtension {
public:
    explicit PitchGardenProcessorEditor(PitchGardenProcessorBase& processorIn)
        : AudioProcessorEditor(&processorIn),AudioProcessorEditorARAExtension(&processorIn),
          transport(processorIn.transport),audition(processorIn.auditionRenderer()),
          piano([this] { return collect(); },[this] { return transport.read(); },
            {[this] { return audition!=nullptr && audition->isPrepared(); },
             [this](const PitchRegionDisplay& region,const pg::Note& note) { return preview(region,note); },
             [this] { if(audition) audition->stop(); previewFailure={}; },
             [this] { return previewStatus(); }}) {
        setWantsKeyboardFocus(false); setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(piano); setResizable(true,false); setResizeLimits(480,260,2400,1600); setSize(1000,560);
    }
    ~PitchGardenProcessorEditor() override { if(audition) audition->stop(); }
    void resized() override { piano.setBounds(getLocalBounds()); }
    bool keyPressed(const KeyPress& key) override {
        if(key.getKeyCode()!=KeyPress::spaceKey || key.getModifiers().isAnyModifierKeyDown()) return false;
        auto* view=getARAEditorView(); if(view==nullptr) return false;
        auto* controller=view->getDocumentController()->getHostPlaybackController();
        const auto state=transport.read(); if(controller==nullptr || !state.available) return false;
        if(!spaceHeld) { if(state.playing) controller->requestStopPlayback(); else controller->requestStartPlayback(); }
        spaceHeld=true; return true;
    }
    bool keyStateChanged(bool) override {
        if(!KeyPress::isKeyCurrentlyDown(KeyPress::spaceKey)) spaceHeld=false;
        return false;
    }
    void focusLost(FocusChangeType) override { spaceHeld=false; }
    AudioProcessorEditorARAExtension* getARAClientExtensions() override { return this; }
private:
    bool preview(const PitchRegionDisplay& region,const pg::Note& note) {
        previewFailure={};
        const auto interval=pg::auditionInterval(region.mapping,note.start,note.end);
        if(audition==nullptr || !region.supported || !interval || transport.read().playing) return false;
        previewRequestedAt=Time::getMillisecondCounterHiRes(); previewCallbacks=audition->callbackCount();
        const bool accepted=audition->submit({region.identity,region.sourceRevision,region.mapping,*interval});
        if(!accepted) previewFailure=audition->isPrepared() ? "Audition unavailable: region not assigned to this ARA editor renderer" : "Audition unavailable: host renderer is not prepared";
        return accepted;
    }
    String previewStatus() const {
        if(audition==nullptr) return "Audition unavailable: host supplied no ARA editor-renderer role";
        if(!audition->isPrepared()) return "Audition waiting for host editor-renderer preparation";
        if(previewFailure.isNotEmpty()) return previewFailure;
        switch(audition->getStatus()) {
            case NoteAuditionStatus::idle: return "Select a note, then Audition (original audio; host transport stays unchanged)";
            case NoteAuditionStatus::queued:
                if(audition->callbackCount()==previewCallbacks && Time::getMillisecondCounterHiRes()-previewRequestedAt>1000)
                    return "Host has not rendered the ARA preview; stopped audition may be unavailable";
                return "Audition queued: waiting for host editor-renderer callback";
            case NoteAuditionStatus::buffering: return "Audition: waiting for cached source samples";
            case NoteAuditionStatus::playing: return "Auditioning original note";
            case NoteAuditionStatus::finished: return "Audition finished";
            case NoteAuditionStatus::invalid: return "Audition cancelled: source or region changed";
            case NoteAuditionStatus::hostPlaying: return "Audition cancelled: host playback started";
        }
        return {};
    }
    std::vector<PitchRegionDisplay> collect() {
        std::vector<PitchRegionDisplay> displays;
        auto* view=getARAEditorView(); if(view==nullptr) return displays;
        auto* dc=view->getDocumentController(); if(dc->isHostEditingDocument()) return displays;
        auto selected=view->getViewSelection().getEffectivePlaybackRegions<ARAPlaybackRegion>();
        if(selected.empty())
            for(auto* sequence:dc->getDocument<ARADocument>()->getRegionSequences()) {
                const auto& hidden=view->getHiddenRegionSequences();
                if(std::find(hidden.begin(),hidden.end(),sequence)==hidden.end())
                    for(auto* region:sequence->getPlaybackRegions()) selected.push_back(region);
            }
        for(auto* region:selected) {
            auto* source=region->getAudioModification()->getAudioSource<AnalyzedSource>();
            const auto map=PitchGardenPlayback::mapping(*region);
            const bool supported=map.supported() && !region->isTimestretchEnabled() && !region->isTimeStretchReflectingTempo();
            const char* name=source->getName();
            displays.push_back({String(name ? name : source->getPersistentID().c_str()),map,source->snapshot(),supported,source->playback.unavailable.load(),static_cast<PitchGardenRegion*>(region)->identity,source->revision.load()});
        }
        return displays;
    }
    pg::TransportState& transport;
    PitchGardenAudition* audition=nullptr;
    double previewRequestedAt=0;
    std::uint64_t previewCallbacks=0;
    String previewFailure;
    PitchEditor piano;
    bool spaceHeld=false;
};
class PitchGardenProcessor final : public PitchGardenProcessorBase {
public:
    bool hasEditor() const override { return true; }
    AudioProcessorEditor* createEditor() override { return new PitchGardenProcessorEditor(*this); }
};
