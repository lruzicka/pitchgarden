#pragma once
#include "PitchAnalysis.h"
#include "NoteSegmenter.h"
#include "PitchQuality.h"
#include <memory>
#include <mutex>
#include <string>
#include <thread>
namespace pg {
enum class AnalysisState { waiting, analyzing, ready, failed };
struct AnalysisResult { double sampleRate=0; std::vector<PitchFrame> frames; std::vector<Note> notes; std::vector<PitchQuality> quality; };
struct AnalysisSnapshot {
    AnalysisState state=AnalysisState::waiting;
    float progress=0;
    std::string error;
    std::shared_ptr<const AnalysisResult> result;
};
// start/cancel/destruction belong to the owner/model thread. snapshot is safe
// on any non-realtime thread. The reader must outlive cancel() returning.
class AnalysisModel {
public:
    ~AnalysisModel();
    void start(double sampleRate, std::int64_t sampleCount, MonoReader);
    void cancel();
    void fail(std::string);
    AnalysisSnapshot snapshot() const;
private:
    mutable std::mutex mutex;
    AnalysisSnapshot value;
    std::atomic<bool> cancelled{false};
    std::thread worker;
};
}
