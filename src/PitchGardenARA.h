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

    ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override
    {
        return new PitchGardenPlayback (getDocumentController(), *this);
    }

    ARAEditorRenderer* doCreateEditorRenderer() noexcept override
    {
        return new ARAEditorRenderer(getDocumentController());
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


struct PlayHeadState
{
    void update (const Optional<AudioPlayHead::PositionInfo>& info)
    {
        if (info.hasValue())
        {
            isPlaying.store (info->getIsPlaying(), std::memory_order_relaxed);
            timeInSeconds.store (info->getTimeInSeconds().orFallback (0), std::memory_order_relaxed);
            isLooping.store (info->getIsLooping(), std::memory_order_relaxed);
            const auto loopPoints = info->getLoopPoints();

            if (loopPoints.hasValue())
            {
                loopPpqStart = loopPoints->ppqStart;
                loopPpqEnd = loopPoints->ppqEnd;
            }
        }
        else
        {
            isPlaying.store (false, std::memory_order_relaxed);
            isLooping.store (false, std::memory_order_relaxed);
        }
    }

    std::atomic<bool> isPlaying { false },
                      isLooping { false };
    std::atomic<double> timeInSeconds { 0.0 },
                        loopPpqStart  { 0.0 },
                        loopPpqEnd    { 0.0 };
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
        playHeadState.update (nullopt);
        prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
    }

    void releaseResources() override
    {
        playHeadState.update (nullopt);
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
        playHeadState.update (audioPlayHead != nullptr ? audioPlayHead->getPosition() : Optional<AudioPlayHead::PositionInfo>{});

        if (! processBlockForARA (buffer, isRealtime(), audioPlayHead))
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

    PlayHeadState playHeadState;

private:
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
          piano([this] { return collect(); }) {
        addAndMakeVisible(piano); setResizable(true,false); setResizeLimits(480,260,2400,1600); setSize(1000,560);
    }
    void resized() override { piano.setBounds(getLocalBounds()); }
    AudioProcessorEditorARAExtension* getARAClientExtensions() override { return this; }
private:
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
            displays.push_back({String(name ? name : source->getPersistentID().c_str()),map,source->snapshot(),supported,source->playback.unavailable.load()});
        }
        return displays;
    }
    PitchEditor piano;
};
class PitchGardenProcessor final : public PitchGardenProcessorBase {
public:
    bool hasEditor() const override { return true; }
    AudioProcessorEditor* createEditor() override { return new PitchGardenProcessorEditor(*this); }
};
