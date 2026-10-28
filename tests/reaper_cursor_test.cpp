#include "ReaperCursorControl.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while(false)
namespace {
const auto uiThread=std::this_thread::get_id();
struct Project { double cursor=0; int moves=0; bool valid=true; } projects[2];
void setCursor(ReaProject* project,double time,bool view,bool seek) {
    CHECK(std::this_thread::get_id()==uiThread); CHECK(!view && !seek);
    auto* p=reinterpret_cast<Project*>(project); p->cursor=time; ++p->moves;
}
double getCursor(ReaProject* project) { return reinterpret_cast<Project*>(project)->cursor; }
bool validate(void* project,const char* type) {
    CHECK(std::this_thread::get_id()==uiThread); CHECK(std::strcmp(type,"ReaProject*")==0);
    return (project==&projects[0] || project==&projects[1]) && static_cast<Project*>(project)->valid;
}
struct Host final : pg::ReaperHostApplication {
    int references=1;
    Project* project=&projects[0];
    bool supported=true,hasSetter=true,hasGetter=true,hasValidation=true;
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,void** result) override {
        *result=nullptr;
        if(supported && std::memcmp(iid,pg::reaperHostIID,16)==0) { *result=static_cast<pg::ReaperHostApplication*>(this); addRef(); return Steinberg::kResultOk; }
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return ++references; }
    Steinberg::uint32 PLUGIN_API release() override { return --references; }
    void* PLUGIN_API getReaperApi(Steinberg::CStringA name) override {
        if(std::strcmp(name,"SetEditCurPos2")==0 && hasSetter) return reinterpret_cast<void*>(&setCursor);
        if(std::strcmp(name,"GetCursorPositionEx")==0 && hasGetter) return reinterpret_cast<void*>(&getCursor);
        if(std::strcmp(name,"ValidatePtr")==0 && hasValidation) return reinterpret_cast<void*>(&validate);
        return nullptr;
    }
    void* PLUGIN_API getReaperParent(Steinberg::uint32 kind) override { CHECK(kind==3); return project; }
    void* PLUGIN_API reaperExtended(Steinberg::uint32,void*,void*,void*) override { return nullptr; }
};
}
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    Host host,other;
    pg::ReaperCursorControl cursor;
    CHECK(!cursor.move(12) && !cursor.position());
    cursor.setIHostApplication(&host); CHECK(host.references==2);
    CHECK(cursor.move(12.3) && projects[0].moves==1 && cursor.position()==12.3);
    CHECK(!cursor.move(std::numeric_limits<double>::quiet_NaN()));
    CHECK(!cursor.move(std::numeric_limits<double>::infinity()));
    CHECK(projects[0].moves==1);
    // Resolve this instance's project, even if a different REAPER tab is active.
    host.project=&projects[1]; CHECK(cursor.move(31));
    CHECK(projects[0].cursor==12.3 && projects[1].cursor==31);
    host.project=nullptr; CHECK(!cursor.move(99) && !cursor.position());
    host.project=&projects[1]; projects[1].valid=false;
    CHECK(!cursor.move(99) && !cursor.position()); projects[1].valid=true;
    bool offThread=false; std::thread worker([&] { offThread=cursor.move(100); CHECK(!cursor.position()); }); worker.join(); CHECK(!offThread);
    CHECK(projects[1].moves==1);
    other.project=&projects[0]; cursor.setIHostApplication(&other); CHECK(host.references==1 && other.references==2);
    CHECK(cursor.move(-1) && cursor.position()==-1);
    other.hasSetter=false; cursor.setIHostApplication(&other); CHECK(!cursor.move(42));
    other.hasSetter=true; other.hasValidation=false; cursor.setIHostApplication(&other); CHECK(!cursor.move(42) && !cursor.position());
    other.hasValidation=true; other.hasGetter=false; cursor.setIHostApplication(&other); CHECK(cursor.move(42) && !cursor.position());
    other.supported=false; cursor.setIHostApplication(&other); CHECK(!cursor.move(42) && other.references==1);
    cursor.setIHostApplication(&host); CHECK(host.references==2);
    cursor.setIHostApplication(nullptr); CHECK(host.references==1 && !cursor.move(1));
    std::cout << "REAPER cursor API flags, project context, validation, lifetime and UI-thread restriction passed\n";
}
