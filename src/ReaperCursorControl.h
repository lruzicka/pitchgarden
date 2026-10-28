#pragma once
#include "JuceHeader.h"
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wshadow")
#include <pluginterfaces/base/funknown.h>
JUCE_END_IGNORE_WARNINGS_GCC_LIKE
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>

class ReaProject;
namespace pg {
// ABI declaration of Cockos' published VST3 host extension (REAPER 5.02+).
// https://github.com/justinfrankel/reaper-sdk/blob/main/sdk/reaper_vst3_interfaces.h
// Keep all three methods, in SDK order. No dependency or SDK files are modified.
class ReaperHostApplication : public Steinberg::FUnknown {
public:
    virtual void* PLUGIN_API getReaperApi(Steinberg::CStringA name)=0;
    virtual void* PLUGIN_API getReaperParent(Steinberg::uint32 kind)=0;
    virtual void* PLUGIN_API reaperExtended(Steinberg::uint32,void*,void*,void*)=0;
};
inline constexpr Steinberg::TUID reaperHostIID=INLINE_UID(0x79655E36,0x77EE4267,0xA573FEF7,0x4912C27C);

// Host initialization may replace the context. GUI callers take a lifetime-safe
// snapshot; no audio callback accesses this object or its mutex. Project context
// is resolved/validated for each GUI operation, never defaulted to the active tab.
class ReaperCursorControl final : public juce::VST3ClientExtensions {
public:
    void setIHostApplication(Steinberg::FUnknown* context) override {
        std::shared_ptr<Binding> next;
        ReaperHostApplication* extension=nullptr;
        if(context && context->queryInterface(reaperHostIID,reinterpret_cast<void**>(&extension))==Steinberg::kResultOk && extension) {
            Steinberg::IPtr<ReaperHostApplication> acquired(extension,false);
            next=std::make_shared<Binding>(std::move(acquired));
            next->set=reinterpret_cast<SetCursor>(extension->getReaperApi("SetEditCurPos2"));
            next->get=reinterpret_cast<GetCursor>(extension->getReaperApi("GetCursorPositionEx"));
            next->validate=reinterpret_cast<Validate>(extension->getReaperApi("ValidatePtr"));
        }
        const std::lock_guard<std::mutex> guard(mutex);
        binding.swap(next);
    }
    bool move(double projectSeconds) const {
        if(!onMessageThread() || !std::isfinite(projectSeconds)) return false;
        const auto current=snapshot();
        auto* project=projectFor(current);
        if(!project || !current->set) return false;
        // Edit cursor only: do not scroll REAPER's arrange view or seek/start
        // its transport. In particular this never implements note audition.
        current->set(project,projectSeconds,false,false);
        return true;
    }
    std::optional<double> position() const {
        if(!onMessageThread()) return {};
        const auto current=snapshot();
        auto* project=projectFor(current);
        if(!project || !current->get) return {};
        const auto seconds=current->get(project);
        return std::isfinite(seconds) ? std::optional<double>(seconds) : std::nullopt;
    }
private:
    using SetCursor=void (*)(ReaProject*,double,bool,bool);
    using GetCursor=double (*)(ReaProject*);
    using Validate=bool (*)(void*,const char*);
    struct Binding {
        explicit Binding(Steinberg::IPtr<ReaperHostApplication> acquired) : host(std::move(acquired)) {}
        Steinberg::IPtr<ReaperHostApplication> host;
        SetCursor set=nullptr;
        GetCursor get=nullptr;
        Validate validate=nullptr;
    };
    static bool onMessageThread() {
        const auto* manager=juce::MessageManager::getInstanceWithoutCreating();
        return manager && manager->isThisTheMessageThread();
    }
    static ReaProject* projectFor(const std::shared_ptr<Binding>& current) {
        if(!current || !current->validate) return nullptr;
        auto* project=static_cast<ReaProject*>(current->host->getReaperParent(3));
        return project && current->validate(project,"ReaProject*") ? project : nullptr;
    }
    std::shared_ptr<Binding> snapshot() const {
        const std::lock_guard<std::mutex> guard(mutex);
        return binding;
    }
    mutable std::mutex mutex;
    std::shared_ptr<Binding> binding;
};
}
