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
#include "ARAPluginDemo.h"
#include "AnalyzedSource.h"

class PitchGardenDocumentController final : public ARADocumentControllerSpecialisation,
                                                            private ProcessingLockInterface
{
public:
    using ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;

    PreviewState previewState;

protected:
    ARAAudioSource* doCreateAudioSource(ARADocument* document, ARA::ARAAudioSourceHostRef hostRef) noexcept override {
        return new AnalyzedSource(document,hostRef);
    }
    void willBeginEditing (ARADocument*) override
    {
        processBlockLock.enterWrite();
    }

    void didEndEditing (ARADocument*) override
    {
        processBlockLock.exitWrite();
        for(auto* source : getDocumentController()->getDocument<ARADocument>()->getAudioSources<AnalyzedSource>())
            source->ensureAnalysis();
    }

    ARAAudioModification* doCreateAudioModification (ARAAudioSource* audioSource,
                                                     ARA::ARAAudioModificationHostRef hostRef,
                                                     const ARAAudioModification* optionalModificationToClone) noexcept override
    {
        return new ARADemoPluginAudioModification (audioSource,
                                                   hostRef,
                                                   static_cast<const ARADemoPluginAudioModification*> (optionalModificationToClone));
    }

    ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override
    {
        return new PlaybackRenderer (getDocumentController(), *this);
    }

    EditorRenderer* doCreateEditorRenderer() noexcept override
    {
        return new EditorRenderer (getDocumentController(), &previewState, *this);
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
            const bool dimmed = input.readBool();

            // Find audio modification to restore the state to (drop state if not to be loaded)
            auto audioModification = filter->getAudioModificationToRestoreStateWithID<ARADemoPluginAudioModification> (persistentID.getCharPointer());

            if (audioModification == nullptr)
                continue;

            const bool dimChanged = (dimmed != audioModification->isDimmed());
            audioModification->setDimmed (false);

            // If the dim state changed, send a sample content change notification without notifying the host
            if (dimChanged)
            {
                audioModification->notifyContentChanged (ARAContentUpdateScopes::samplesAreAffected(), false);

                for (auto playbackRegion : audioModification->getPlaybackRegions())
                    playbackRegion->notifyContentChanged (ARAContentUpdateScopes::samplesAreAffected(), false);
            }
        }

        getDocumentController()->getHostArchivingController()->notifyDocumentUnarchivingProgress (1.0f);

        return ! input.failed();
    }

    bool doStoreObjectsToStream (ARAOutputStream& output, const ARAStoreObjectsFilter* filter) noexcept override
    {
        // This example implementation only deals with audio modification states
        const auto& audioModificationsToPersist { filter->getAudioModificationsToStore<ARADemoPluginAudioModification>() };

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

            if (! output.writeBool (audioModificationsToPersist[i]->isDimmed()))
                return false;

            const auto progressVal = (float) i / (float) numAudioModifications;
            reportProgress (progressVal);
        }

        return true;
    }

private:
    ScopedTryReadLock getProcessingLock() override
    {
        return ScopedTryReadLock { processBlockLock };
    }

    ReadWriteLock processBlockLock;
};


class PitchGardenProcessor final : public ARADemoPluginAudioProcessorImpl {
public:
    const String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool hasEditor() const override { return true; }
    AudioProcessorEditor* createEditor() override { return new GenericAudioProcessorEditor(*this); }
};
