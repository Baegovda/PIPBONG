#include "core/workflow/HoldKeyTapMultiplexer.h"
#include "core/workflow/WorkflowEngine.h"
#include "core/vision/ImageMatcher.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QThread>
#include <QTimer>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <iostream>
#include <vector>

namespace {
bool waitFor(const std::function<bool()>& condition, int timeoutMs = 5000) {
    QElapsedTimer deadline; deadline.start();
    while (!condition() && deadline.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return condition();
}
double percentile(std::vector<double> values, double fraction) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    return values.at(static_cast<size_t>((values.size() - 1) * fraction));
}
class MatchWorkload : public Block {
public:
    MatchWorkload(const cv::Mat& hay, const cv::Mat& needle) : m_hay(hay) {
        m_template.bgr = needle;
        ImageMatcher::toGrayscale(needle, m_template.gray);
    }
    // This CPU workload emits no match-preview UI and can run under QCoreApplication.
    BlockType type() const override { return BlockType::Wait; }
    std::string displayName() const override { return "Matcher workload"; }
    std::string summary() const override { return displayName(); }
    BlockResult execute(ExecutionContext& ctx) override {
        for (int i = 0; i < 20; ++i) {
            if (ctx.shouldStop()) return {false, "cancelled"};
            QElapsedTimer lap; lap.start();
            const MatchResult result = ImageMatcher::findPeakMatchBgr(m_hay, m_template, {});
            samples.push_back(lap.nsecsElapsed() / 1000000.0);
            if (!result.found || result.confidence < 0.9) return {false, "match failed"};
        }
        return {};
    }
    nlohmann::json toJson() const override { return {}; }
    std::unique_ptr<Block> clone() const override { return std::make_unique<MatchWorkload>(m_hay, m_template.bgr); }
    std::vector<double> samples;
private:
    cv::Mat m_hay;
    PreparedTemplate m_template;
};
}

bool runRuntimeStressChecks() {
    std::array<std::atomic<int>, 4> taps{};
    std::array<std::atomic<bool>, 4> held{};
    for (int i = 0; i < 4; ++i) { taps[i].store(0); held[i].store(true); }
    const std::array<int, 4> keys{0x51,0x57,0x45,0x52};
    // Real scheduler, in-memory emission only. No OS key/mouse events are sent.
    HoldKeyTapMultiplexer mux(nullptr, [&](int key, ExecutionContext&) {
        for (int i = 0; i < 4; ++i) if (keys[i] == key) ++taps[i];
    });
    for (int i = 0; i < 4; ++i) {
        mux.startLane(std::to_string(i), keys[i], 3, [&, i] { return held[i].load(); },
                      std::make_shared<ExecutionContext>());
    }
    if (!waitFor([&] { return std::all_of(taps.begin(), taps.end(), [](const auto& n) { return n.load() >= 5; }); })) return false;
    held[0].store(false);
    if (!waitFor([&] { return !mux.isLaneActive("0"); })) return false;
    const int remainingBefore = taps[1].load();
    if (!waitFor([&] { return taps[1].load() > remainingBefore + 3; }) || !mux.isLaneActive("1")) return false;
    QElapsedTimer cancellation; cancellation.start();
    mux.stopAll();
    if (!waitFor([&] { return !mux.hasAnyActiveLane(); })) return false;
    const qint64 cancelMs = cancellation.elapsed();

    cv::Mat hay(540,960,CV_8UC3);
    cv::RNG random(0x8b19); random.fill(hay, cv::RNG::UNIFORM, 0, 255);
    const cv::Mat needle = hay(cv::Rect(300,200,48,48)).clone();
    nlohmann::json scenarios = nlohmann::json::array();
    const int previousThreads = cv::getNumThreads();
    cv::setNumThreads(1);
    for (int count : {1,4}) {
        std::vector<std::unique_ptr<WorkflowEngine>> engines;
        std::vector<std::shared_ptr<Workflow>> workflows;
        std::vector<MatchWorkload*> workloads;
        std::vector<std::shared_ptr<ExecutionContext>> contexts;
        int completed = 0;
        bool success = true;
        QElapsedTimer pulseClock; pulseClock.start();
        qint64 lastPulse = 0;
        std::vector<double> heartbeatGaps;
        QTimer heartbeat;
        heartbeat.setInterval(10);
        QObject::connect(&heartbeat, &QTimer::timeout, &heartbeat, [&] {
            const qint64 now = pulseClock.elapsed();
            heartbeatGaps.push_back(now-lastPulse); lastPulse = now;
        });
        heartbeat.start();
        QElapsedTimer wall; wall.start();
        for (int i = 0; i < count; ++i) {
            auto workflow = std::make_shared<Workflow>();
            auto work = std::make_unique<MatchWorkload>(hay, needle);
            workloads.push_back(work.get()); workflow->addBlock(std::move(work));
            auto engine = std::make_unique<WorkflowEngine>();
            QObject::connect(engine.get(), &WorkflowEngine::finished, &heartbeat,
                [&](bool ok, const QString&) { success = success && ok; ++completed; });
            auto context = std::make_shared<ExecutionContext>();
            contexts.push_back(context);
            engine->run(workflow, context);
            engines.push_back(std::move(engine)); workflows.push_back(workflow);
        }
        const bool done = waitFor([&] { return completed == count; }, 10000);
        heartbeat.stop();
        const qint64 elapsed = wall.elapsed();
        std::vector<double> samples;
        for (auto& engine : engines) if (!engine->stopAndWaitBounded(2000)) success = false;
        for (const auto* workload : workloads) samples.insert(samples.end(), workload->samples.begin(), workload->samples.end());
        if (!done || !success || samples.size() != static_cast<size_t>(count * 20)) { cv::setNumThreads(previousThreads); return false; }
        scenarios.push_back({{"sessions",count},{"matches",samples.size()},{"wall_ms",elapsed},
            {"match_p50_ms",percentile(samples,0.50)},{"match_p95_ms",percentile(samples,0.95)},
            {"event_loop_gap_p95_ms",percentile(heartbeatGaps,0.95)}});
    }
    cv::setNumThreads(previousThreads);
    const nlohmann::json report{{"timestamp_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString()},
        {"workload","Synthetic 960x540 BGR image / 48x48 template, 20 matches per WorkflowEngine; OpenCV threads=1. No native input or foreground transitions."},
        {"hold_lanes",4},{"hold_stop_all_ms",cancelMs},{"scenarios",scenarios}};
    const QString path = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("runtime-perf.json"));
    QFile file(path); if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QByteArray::fromStdString(report.dump(2)));
    std::cout << "Runtime stress: 4 hold lanes isolated + 1/4 matcher engines OK (synthetic; no native input)\n";
    return true;
}
