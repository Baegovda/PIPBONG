#include "core/diagnostics/AppStutterProfiler.h"
#include "core/diagnostics/CrashReporter.h"
#include "core/vision/TemplateCache.h"
#include "core/workflow/WorkflowEngine.h"
#include "core/workflow/blocks/WaitBlock.h"
#include "model/Project.h"
#include "app/UpdateChecker.h"
#include "storage/JsonSerializer.h"
#include "storage/ProjectHistoryStore.h"
#include "ui/calculator/FormulaEvaluator.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <opencv2/imgcodecs.hpp>
#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <thread>

// Headless test binary: production execution, without crash-report side effects.
void CrashReporter::noteBreadcrumb(const QString&, const QString&) {}
AppStutterOperationScope::AppStutterOperationScope(const char*, const QString&, const char*) {}
AppStutterOperationScope::~AppStutterOperationScope() {}

namespace {
bool expect(bool condition, const char* label) {
    if (!condition) std::cerr << "Reliability FAIL: " << label << '\n';
    return condition;
}
bool waitUntil(const std::function<bool()>& condition) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 2000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return condition();
}
class SlowBlock : public WaitBlock {
public:
    explicit SlowBlock(std::atomic<bool>& executing) : m_executing(executing) {}
    BlockResult execute(ExecutionContext&) override {
        m_executing.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_executing.store(false);
        return {};
    }
private:
    std::atomic<bool>& m_executing;
};
}

bool runReliabilityChecks() {
    const QByteArray payload("verified-update");
    UpdateChecker::ReleaseInfo release;
    release.assetSizeBytes = payload.size();
    release.assetSha256 = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
    if (!expect(UpdateChecker::verifyDownload(release, payload), "verified update payload")) return false;
    if (!expect(!UpdateChecker::verifyDownload(release, payload + 'x'), "wrong update size rejected")) return false;
    release.assetSha256 = QString(64, QLatin1Char('0'));
    if (!expect(!UpdateChecker::verifyDownload(release, payload), "wrong update digest rejected")) return false;
    release.assetSha256.clear();
    if (!expect(!UpdateChecker::verifyDownload(release, payload), "missing update digest rejected")) return false;
    const auto noCell = [](int, int) -> std::optional<double> { return {}; };
    const auto valid = FormulaEvaluator::evaluate(QStringLiteral("=2+3*4  "), noCell);
    if (!expect(valid.ok && valid.value == 14, "valid formula")) return false;
    for (const char* text : {"=1+2garbage", "=1 2", "=1..2", "=1/0"}) {
        if (!expect(!FormulaEvaluator::evaluate(QString::fromLatin1(text), noCell).ok,
                    "invalid formula rejected")) return false;
    }

    QTemporaryDir dir;
    if (!expect(dir.isValid(), "temporary directory")) return false;
    const QString path = dir.filePath(QStringLiteral("project.json"));
    for (const char* document : {"{", "[]", "{\"features\":false}",
                                "{\"version\":2}", "{\"features\":[{\"repeatCount\":\"oops\"}]}"}) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        file.write(document);
        file.close();
        QString projectDirectory = QStringLiteral("unchanged");
        if (!expect(!JsonSerializer::loadFromFile(path, &projectDirectory)
                    && projectDirectory == QStringLiteral("unchanged"),
                    "invalid project fails without mutating output")) return false;
    }
    Project project;
    project.addFeature("roundtrip");
    if (!expect(JsonSerializer::saveToFile(project, path, dir.path()), "atomic project save")) return false;
    auto loaded = JsonSerializer::loadFromFile(path);
    if (!expect(loaded && loaded->features().size() == 1
                && loaded->featureAt(0)->name() == "roundtrip", "project roundtrip")) return false;
    if (!expect(!JsonSerializer::saveToFile(project, dir.path(), dir.path()),
                "save to directory fails")) return false;
    for (int i = 0; i < 12; ++i) {
        project.featureAt(0)->setName("version-" + std::to_string(i));
        if (!expect(JsonSerializer::saveToFile(project, path, dir.path()), "save with recovery snapshot")) return false;
    }
    const QStringList history = ProjectHistoryStore::snapshots(path);
    if (!expect(history.size() == ProjectHistoryStore::kMaxSnapshots, "history retention limit")) return false;
    auto previous = JsonSerializer::loadFromFile(history.first());
    if (!expect(previous && previous->featureAt(0)->name() == "version-10", "most recent snapshot ordered correctly")) return false;
    if (!expect(JsonSerializer::saveToFile(*previous, path, dir.path()), "restore snapshot safely")) return false;
    auto restored = JsonSerializer::loadFromFile(path);
    if (!expect(restored && restored->featureAt(0)->name() == "version-10", "restored content")) return false;
    bool originalRetained = false;
    for (const QString& snapshot : ProjectHistoryStore::snapshots(path)) {
        auto saved = JsonSerializer::loadFromFile(snapshot);
        if (saved && saved->featureAt(0)->name() == "version-11") originalRetained = true;
    }
    if (!expect(originalRetained, "pre-restore version preserved")) return false;

    const QString blockedPath = dir.filePath(QStringLiteral("blocked.json"));
    if (!JsonSerializer::saveToFile(project, blockedPath, dir.path())) return false;
    QFile blockedHistory(ProjectHistoryStore::directoryFor(blockedPath));
    if (!blockedHistory.open(QIODevice::WriteOnly)) return false;
    blockedHistory.write("not a directory");
    blockedHistory.close();
    project.featureAt(0)->setName("must-not-replace");
    if (!expect(!JsonSerializer::saveToFile(project, blockedPath, dir.path()), "history failure aborts save")) return false;
    auto unchanged = JsonSerializer::loadFromFile(blockedPath);
    if (!expect(unchanged && unchanged->featureAt(0)->name() == "version-11", "history failure preserves current file")) return false;

    cv::Mat patch(40, 40, CV_8UC3);
    cv::randu(patch, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));
    const std::string templatePath = dir.filePath(QStringLiteral("match.png")).toStdString();
    cv::imwrite(templatePath, patch);
    auto first = TemplateCache::getOrLoad(templatePath);
    auto second = TemplateCache::getOrLoad(templatePath);
    if (!expect(first != second && first->bgr.data == second->bgr.data,
                "scale caches isolated; immutable image buffers shared")) return false;
    auto search = [patch](const std::shared_ptr<PreparedTemplate>& templ, double minScale) {
        MatchOptions options;
        options.multiScale = true;
        options.minScale = minScale;
        options.maxScale = 1.1;
        for (int i = 0; i < 20; ++i) {
            if (!ImageMatcher::findPeakMatchBgr(patch, *templ, options).found) return false;
        }
        return true;
    };
    auto one = std::async(std::launch::async, search, first, 0.9);
    auto two = std::async(std::launch::async, search, second, 0.8);
    if (!expect(one.get() && two.get(), "concurrent template searches")) return false;

    WorkflowEngine engine;
    auto workflow = std::make_shared<Workflow>();
    auto block = std::make_unique<WaitBlock>(); block->ms = 0;
    workflow->addBlock(std::move(block));
    auto context = std::make_shared<ExecutionContext>();
    std::weak_ptr<ExecutionContext> weak = context;
    bool finished = false;
    QObject::connect(&engine, &WorkflowEngine::finished, &engine,
                     [&](bool, const QString&) { finished = true; });
    engine.run(workflow, context);
    if (!expect(waitUntil([&] { return finished; }), "engine completes")) return false;
    context.reset();
    if (!expect(waitUntil([&] { return weak.expired(); }), "finished context released")) return false;
    if (!expect(engine.stopAndWaitBounded(1000), "idle worker shutdown")) return false;

    std::atomic<bool> executing{false};
    auto slow = std::make_shared<Workflow>();
    slow->addBlock(std::make_unique<SlowBlock>(executing));
    engine.run(slow, std::make_shared<ExecutionContext>());
    if (!expect(waitUntil([&] { return executing.load(); }), "slow block starts")) return false;
    if (!expect(!engine.stopAndWaitBounded(1) && engine.hasLiveWorker(),
                "timed out worker remains tracked")) return false;
    if (!expect(engine.stopAndWaitBounded(1000) && !engine.hasLiveWorker(),
                "timed out worker eventually joins")) return false;

    auto cancelled = std::make_shared<ExecutionContext>();
    engine.run(workflow, cancelled);
    engine.stop();
    if (!expect(engine.stopAndWaitBounded(1000) && cancelled->shouldStop(),
                "stop immediately after queue is retained")) return false;
    QCoreApplication::processEvents();
    std::cout << "Reliability: formula/project/cache/engine lifetime + cancellation/update digest OK\n";
    return true;
}
