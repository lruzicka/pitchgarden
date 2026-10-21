#include "AnalysisModel.h"
namespace pg {
AnalysisModel::~AnalysisModel() { cancel(); }
AnalysisSnapshot AnalysisModel::snapshot() const { std::lock_guard<std::mutex> guard(mutex); return value; }
void AnalysisModel::cancel() {
    cancelled.store(true);
    if(worker.joinable()) worker.join();
    std::lock_guard<std::mutex> guard(mutex); value={};
}
void AnalysisModel::fail(std::string error) {
    cancel(); std::lock_guard<std::mutex> guard(mutex);
    value.state=AnalysisState::failed; value.error=std::move(error);
}
void AnalysisModel::start(double sr, std::int64_t count, MonoReader read) {
    cancel();
    cancelled.store(false);
    { std::lock_guard<std::mutex> guard(mutex); value.state=AnalysisState::analyzing; }
    worker=std::thread([this,sr,count,read=std::move(read)] {
        try {
            auto result=std::make_shared<AnalysisResult>(); result->sampleRate=sr;
            result->frames=analyzeSource(sr,count,read,{},&cancelled,[this](float p) {
                std::lock_guard<std::mutex> guard(mutex); value.progress=p;
            });
            std::lock_guard<std::mutex> guard(mutex);
            if(!cancelled.load()) { value.result=std::move(result); value.state=AnalysisState::ready; value.progress=1; }
        } catch(const std::exception& e) {
            std::lock_guard<std::mutex> guard(mutex);
            if(!cancelled.load()) { value.state=AnalysisState::failed; value.error=e.what(); }
        } catch(...) {
            std::lock_guard<std::mutex> guard(mutex);
            if(!cancelled.load()) { value.state=AnalysisState::failed; value.error="Unknown analysis failure"; }
        }
    });
}
}
