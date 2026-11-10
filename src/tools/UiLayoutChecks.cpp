bool runWorkspaceEditorChecks();
bool runPointerFeedbackChecks();
bool runDocumentActionsChecks();
bool runWindowDiagnosticsChecks();
bool runProgramSettingsChecks();
bool runWindowChromeChecks();
bool runHoldTapSessionChecks();
bool runRunSessionResourcesChecks();
bool runFirstTemplateRoiChecks();
bool runRoiPreviewOwnershipChecks();
bool runMainWorkspaceChecks();
bool runWindowShellChecks();
bool runAuxiliaryToolsChecks();
#include "ui/WorkflowEditorPanel.h"
#include "ui/FeatureListPanel.h"
#include "ui/UiStateManager.h"
#include <QListWidget>
#include "app/MainWindow.h"
#include "app/RunLifecycleCoordinator.h"
#include "app/RunContextController.h"
#include <QEvent>
#include "model/Project.h"
#include "storage/JsonSerializer.h"
#include <QLabel>
#include <QElapsedTimer>
#include <QThread>
#include "ui/memo/MemoDialog.h"
#include "storage/ProfileMemoStore.h"
#include "ui/BlockListWidget.h"
#include "model/Feature.h"
#include "core/workflow/blocks/WaitBlock.h"
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTextEdit>
#include <iostream>

bool runFeatureTransferChecks();
bool runWorkspaceHistoryUiChecks();
bool runWorkspaceHistoryActionsChecks();
bool runProjectWorkspaceUiChecks();
bool runTargetWindowDetailsUiChecks(const QString& folder);
bool runFeatureLibraryActionsChecks();
bool runTargetWindowActionsChecks(const QString& folder);
bool runTargetCenterPinUiChecks(const QString& folder);
bool runCaptureTargetUiChecks(const QString& folder);
bool runInputInterruptChecks();
bool runPassiveDiagnosticsChecks();
bool runInputInterruptUiChecks();
bool runSessionMouseLockUiChecks();
bool runForegroundSessionChecks();
bool runForegroundSessionUiChecks();
bool runSessionInputCleanupUiChecks();
bool runProfileListUiChecks();
bool runFeatureBatchChecks();
bool runFeatureBatchUiChecks();
bool runProfileMutationChecks();
bool runProfileRecoveryChecks();
bool runProfileActionsChecks();
bool runProfileActionsUiChecks();
bool runProfileStorageEdgesChecks();
bool runFeatureHotkeyChecks();
bool runTriggerMonitorChecks();
bool runTriggerPreemptionChecks();
bool runWorkflowExecutionChecks();
bool runWorkflowToolbarChecks();

// Exercises queued production signal connections against a replaced engine.
class RunLifecycleBoundaryChecks {
public:
    static bool run(MainWindow& window) {
        auto* feature = window.m_project->featureById("run-ui-integration");
        if (!feature) return false;
        const QString originalProfile = window.m_profileManager->activeProfileId();
        const QString owner = window.m_profileManager->addProfile(QStringLiteral("boundary owner"));
        if (owner.isEmpty() || !window.m_profileManager->setActiveProfile(originalProfile)) return false;
        FeatureRunSession owned;
        owned.profileId = owner;
        owned.featureId = "missing-owner-feature";
        owned.sessionContext = std::make_shared<ExecutionContext>();
        window.syncRunSessionContext(owned, false);
        const bool ownerDirectory = owned.sessionContext->projectDirectory()
            == window.m_profileManager->projectDirectory(owner).toStdString();
        if (!window.m_profileManager->removeProfile(owner) || !ownerDirectory) return false;
        Feature monitorFeature("monitor gate probe");
        monitorFeature.setRunMode(FeatureRunMode::Trigger);
        FeatureRunSession monitor;
        monitor.featureId = "boundary-monitor";
        monitor.runningMode = FeatureRunMode::Trigger;
        monitor.triggerPhase = TriggerSessionPhase::Monitoring;
        monitor.sessionContext = std::make_shared<ExecutionContext>();
        RunContextController::apply(monitor, monitorFeature);
        const auto monitorContext = monitor.sessionContext;
        FeatureRunSession capture;
        capture.featureId = "boundary-capture";
        capture.engine = std::make_unique<WorkflowEngine>();
        auto captureWorkflow = std::make_shared<Workflow>();
        auto captureWait = std::make_unique<WaitBlock>(); captureWait->ms = 5000;
        captureWorkflow->addBlock(std::move(captureWait));
        capture.engine->run(captureWorkflow, std::make_shared<ExecutionContext>());
        auto* captureEngine = capture.engine.get();
        window.runSessions().emplace(monitor.featureId, std::move(monitor));
        window.runSessions().emplace(capture.featureId, std::move(capture));
        window.synchronizeRunWorkerState();
        const bool yielded = !monitorContext->triggerMonitorCaptureAllowed()
            && !monitorContext->scopedTargetPollAllowed(); // Missing owner fails closed.
        captureEngine->stopAndWaitBounded(1000);
        window.synchronizeRunWorkerState();
        const bool resumed = monitorContext->triggerMonitorCaptureAllowed();
        window.runSessions().erase("boundary-monitor");
        window.runSessions().erase("boundary-capture");
        if (!yielded || !resumed) return false;
        auto retired = RunLifecycleCoordinator::prepareNewSession(window, *feature, {}, false, true);
        auto current = RunLifecycleCoordinator::prepareNewSession(window, *feature, {}, false, true);
        auto* oldEngine = retired.session.engine.get();
        const auto id = feature->id();
        current.session.repeatRemaining = 17;
        current.session.runningBlockIndex = 2;
        current.session.sessionContext = std::make_shared<ExecutionContext>();
        current.session.sessionWorkflow = std::make_shared<Workflow>();
        const auto currentContext = current.session.sessionContext;
        const auto currentWorkflow = current.session.sessionWorkflow;
        window.runSessions().emplace(id, std::move(current.session));
        window.m_abandonedEngineFeatureIds[oldEngine] = id;
        window.m_abandonedEngines.push_back(std::move(retired.session.engine));
        auto* active = window.sessionFor(id);
        if (RunLifecycleCoordinator::sessionForEngine(window, oldEngine)
            || RunLifecycleCoordinator::sessionForEngine(window, active->engine.get()) != active) return false;
        emit oldEngine->sessionPrepared(std::make_shared<Workflow>(), std::make_shared<ExecutionContext>());
        emit oldEngine->blockStarted(7, QStringLiteral("retired"));
        emit oldEngine->finished(true, QStringLiteral("retired completion"));
        QCoreApplication::sendPostedEvents(&window, QEvent::MetaCall);
        active = window.sessionFor(id);
        const bool untouched = active && active->sessionContext == currentContext
            && active->sessionWorkflow == currentWorkflow
            && active->repeatRemaining == 17 && active->runningBlockIndex == 2;
        if (!untouched) { window.stopAllSessions(); return false; }
        // Ignoring retired signals must still permit the ordinary stop/drain path.
        active->userStopRequested = true;
        RunLifecycleCoordinator::abandonSessionEngine(window, *active);
        RunLifecycleCoordinator::pruneAbandonedEngines(window);
        const bool finalized = !window.sessionFor(id);
        QCoreApplication::processEvents();
        return finalized;
    }
};

int runUiLayoutChecks() {
    if (QApplication::platformName() != QStringLiteral("offscreen")) return 2;
    // The offscreen platform has no Windows font fallback. Load the installed
    // Korean font for readable diagnostic renders without changing product fonts.
    const int fontId = QFontDatabase::addApplicationFont(
        QDir(qEnvironmentVariable("WINDIR")).filePath(QStringLiteral("Fonts/malgun.ttf")));
    const auto families = QFontDatabase::applicationFontFamilies(fontId);
    if (!families.isEmpty()) QApplication::setFont(QFont(families.front(), 9));
    if (!runInputInterruptChecks()) return 39;
    if (!runPassiveDiagnosticsChecks()) return 42;
    if (!runForegroundSessionChecks()) return 43;
    QSettings settings;
    // Reused isolated test data must not reopen a prior run's focused memo
    // while unrelated hotkey fixtures are exercising their dispatch gates.
    // Memo persistence itself is covered with explicit preferences below.
    settings.beginGroup(QStringLiteral("memo/open"));settings.remove(QString());settings.endGroup();
    settings.setValue(QStringLiteral("workflowEditor/detailedColumns"), false);
    const QString folder = qEnvironmentVariable("PIPBONG_DATA_DIR");
    Feature feature("표시 검사");
    auto wait = std::make_unique<WaitBlock>();
    wait->ms = 250;
    feature.workflow().addBlock(std::move(wait));
    WorkflowEditorPanel panel;
    panel.resize(1100, 560);
    panel.setFeature(&feature);
    panel.show();
    QApplication::processEvents();
    auto* table = panel.findChild<BlockListWidget*>();
    auto* toggle = panel.findChild<QCheckBox*>(QStringLiteral("workflowDetailedColumns"));
    if (!table || !toggle || toggle->isChecked()) return 3;
    const auto coreVisible = [&] {
        for (int column : {0,1,2,3,4,11}) if (table->isColumnHidden(column)) return false;
        return true;
    };
    if (!coreVisible()) return 4;
    for (int column = 5; column <= 10; ++column) if (!table->isColumnHidden(column)) return 5;
    BlockListColumnLayout layout = table->columnLayout();
    layout.scoreWidth = 100;
    layout.matchDurationWidth = 86;
    table->setColumnLayout(layout, false);
    table->setRoiCorrectionColumnVisible(true);
    if (!table->isColumnHidden(10)) return 6;
    panel.grab().save(QDir(folder).filePath(QStringLiteral("ui-basic.png")));
    toggle->setChecked(true);
    QApplication::processEvents();
    if (!coreVisible() || !table->detailedColumnsVisible()) return 7;
    for (int column = 5; column <= 10; ++column) if (table->isColumnHidden(column)) return 8;
    if (table->columnLayout().scoreWidth != 100 || table->columnLayout().matchDurationWidth != 86) return 9;
    if (!QSettings().value(QStringLiteral("workflowEditor/detailedColumns")).toBool()) return 10;
    panel.grab().save(QDir(folder).filePath(QStringLiteral("ui-details.png")));
    table->setRoiCorrectionColumnVisible(false);
    if (!table->isColumnHidden(10)) return 11;
    toggle->setChecked(false);
    QApplication::processEvents();
    if (!coreVisible() || table->columnLayout().scoreWidth != 100) return 12;
    panel.setFeature(nullptr);
    QApplication::processEvents();
    const QList<int> paneSizes = panel.workflowSplitter()->sizes();
    if (!panel.isEnabled() || paneSizes.size() != 2 || paneSizes.at(0) < 120 || paneSizes.at(1) < 88) return 13;
    BlockListWidget narrow;
    narrow.resize(420, 240);
    narrow.setDetailedColumnsVisible(true);
    narrow.show();
    QApplication::processEvents();
    if (narrow.horizontalScrollBar()->maximum() <= 0) return 14;
    QFile report(QDir(folder).filePath(QStringLiteral("ui-report.txt")));
    if (!report.open(QIODevice::WriteOnly)) return 15;
    report.write("PASS: basic/detail columns, saved preference, metric width retention, ROI gating, empty splitter and narrow horizontal scroll. Offscreen Qt test; no native input.\n");
    std::cout << "Workflow UI layout: basic/detail + preference + width/ROI + narrow layout OK\n";
    {
        QSettings settings;
        const QString key=UiStateManager::settingsKey("featureList/libraryVertical_v1");
        const QString expanded="ui/state/featureList/libraryDrawerExpanded";
        const auto prior=settings.value(key), priorExpanded=settings.value(expanded);
        settings.remove(key);settings.setValue(expanded,true);
        FeatureListPanel library;
        library.resize(360,580);
        library.setLibraryEntries({{"one","첫 항목",0},{"two","둘째 항목",0},{"three","셋째 항목",0}});
        library.show();QApplication::processEvents();
        auto* list=library.findChild<QListWidget*>("featureLibraryList");
        const bool allRows=list && list->count()==3 && list->visualItemRect(list->item(2)).bottom()<list->viewport()->height();
        const int available=library.featureLibrarySplitter()->height()-library.featureLibrarySplitter()->handleWidth();
        library.featureLibrarySplitter()->setSizes({available-76,76});
        settings.setValue(key,library.featureLibrarySplitter()->saveState());
        FeatureListPanel restored;restored.resize(360,580);restored.show();
        UiStateManager state;state.registerSplitter(restored.featureLibrarySplitter(),"featureList/libraryVertical_v1");state.restoreAll();
        const auto chosen=restored.featureLibrarySplitter()->sizes();
        QApplication::processEvents();
        const bool preserved=restored.featureLibrarySplitter()->sizes()==chosen;
        if(prior.isValid())settings.setValue(key,prior);else settings.remove(key);
        if(priorExpanded.isValid())settings.setValue(expanded,priorExpanded);else settings.remove(expanded);
        if(!allRows || !preserved){std::cerr<<"Library initial layout FAILED rows="<<allRows<<" saved="<<preserved<<'\n';return 49;}
        std::cout<<"Library initial layout: three rows visible after first layout, persisted small pane preserved OK (offscreen)\n";
    }
#ifdef Q_OS_WIN
    // The real window shell is built offscreen with isolated DATA_DIR settings.
    // Lock its actual project file, invoke its dirty slot and call the same
    // persistence gate used by closeEvent. No native input or window operations.
    HANDLE locked = INVALID_HANDLE_VALUE;
    const QString projectPath = QDir(folder).filePath(QStringLiteral("profiles/default/project.json"));
    {
    MainWindow window;
    QFile original(projectPath);
    if (!original.open(QIODevice::ReadOnly)) return 16;
    const QByteArray before = original.readAll(); original.close();
    locked = CreateFileW(reinterpret_cast<LPCWSTR>(projectPath.utf16()), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (locked == INVALID_HANDLE_VALUE) return 17;
    const bool dirtied = QMetaObject::invokeMethod(&window, "onProjectModified", Qt::DirectConnection);
    const bool preventedClose = !window.prepareForShutdown();
    CloseHandle(locked);
    QFile unchanged(projectPath);
    if (!unchanged.open(QIODevice::ReadOnly)) return 18;
    const bool preserved = unchanged.readAll() == before; unchanged.close();
    if (!dirtied || !preventedClose || !preserved) return 19;
    if (!window.prepareForShutdown() || !window.prepareForShutdown()) return 20;
    std::cout << "MainWindow shutdown integration: actual locked project blocks close, original retained, unlocked retry/idempotence OK (offscreen)\n";
    }
    Project repeatProject;
    auto* repeated = repeatProject.addFeature("반복 UI 연결 검사");
    repeated->setId("run-ui-integration");
    repeated->setRepeatCount(300);
    repeated->setPointerVisualFeedback(false);
    auto repeatedWait = std::make_unique<WaitBlock>(); repeatedWait->ms = 0;
    repeated->workflow().addBlock(std::move(repeatedWait));
    if (!JsonSerializer::saveToFile(repeatProject, projectPath, QFileInfo(projectPath).absolutePath())) return 28;
    {
        MainWindow repeatedWindow;
        auto* repeatedList=repeatedWindow.findChild<FeatureListPanel*>();
        if(!repeatedList)return 29;
        repeatedList->selectFeatureById(QStringLiteral("run-ui-integration"));
        auto* completedLoops = repeatedWindow.findChild<QLabel*>(QStringLiteral("workflowRunStatLoop"));
        if (!completedLoops || !QMetaObject::invokeMethod(&repeatedWindow, "onFeatureRunRequested", Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("run-ui-integration")))) return 29;
        QElapsedTimer deadline; deadline.start();
        while (completedLoops->text() != QStringLiteral("루프 300") && deadline.elapsed() < 4000) {
            QCoreApplication::processEvents(); QThread::msleep(1);
        }
        if (completedLoops->text() != QStringLiteral("루프 300")) {
            std::cerr<<"Repeat UI integration FAILED: selected="<<repeatedList->selectedFeatureId().toStdString()
                <<" label="<<completedLoops->text().toStdString()<<" elapsed="<<deadline.elapsed()<<'\n';return 30;
        }
        repeatedWindow.grab().save(QDir(folder).filePath(QStringLiteral("ui-run-completed.png")));
        if (!RunLifecycleBoundaryChecks::run(repeatedWindow)) return 32;
        if (!repeatedWindow.prepareForShutdown()) return 31;
    }
    std::cout << "MainWindow repeat UI integration: actual 300-repeat workflow publishes final loop 300 through publisher/controller wiring OK (offscreen; Wait-only)\n";
    std::cout << "MainWindow worker boundary: real-engine capture yield/resume, missing-owner gate, retired prepared/block/finished signal isolation and deferred-stop finalization OK (offscreen)\n";
    QTemporaryDir memoData;
    const QString one = memoData.filePath(QStringLiteral("one"));
    const QString two = memoData.filePath(QStringLiteral("two"));
    if (!ProfileMemoStore::save(one, QStringLiteral("one")) || !ProfileMemoStore::save(two, QStringLiteral("two"))) return 21;
    MemoDialog memo;
    if (!memo.setProfile(QStringLiteral("one"), one, QStringLiteral("첫 프로필"))) return 22;
    auto* editor = memo.findChild<QTextEdit*>();
    if (!editor) return 23;
    editor->setPlainText(QStringLiteral("unsaved"));
    const QString memoPath = ProfileMemoStore::memoFilePath(one);
    locked = CreateFileW(reinterpret_cast<LPCWSTR>(memoPath.utf16()), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (locked == INVALID_HANDLE_VALUE) return 24;
    const bool memoPreserved = !memo.saveNow() && !memo.setProfile(QStringLiteral("two"), two, QStringLiteral("둘째 프로필"))
        && editor->toPlainText() == QStringLiteral("unsaved") && memo.windowTitle().contains(QStringLiteral("첫 프로필"));
    CloseHandle(locked);
    if (!memoPreserved || !memo.setProfile(QStringLiteral("two"), two, QStringLiteral("둘째 프로필"))) return 25;
    QString stored;
    if (!ProfileMemoStore::load(one, &stored) || stored != QStringLiteral("unsaved") || editor->toPlainText() != QStringLiteral("two")) return 26;
    std::cout << "Memo integration: locked save preserves prior profile/text, unlocked retry saves before switching OK (offscreen)\n";
#endif
    if (!runFeatureTransferChecks()) return 27;
    if (!runWorkspaceHistoryUiChecks()) return 33;
    if (!runWorkspaceHistoryActionsChecks()) return 74;
    if (!runProjectWorkspaceUiChecks()) return 34;
    if (!runTargetWindowDetailsUiChecks(folder)) return 35;
    if (!runTargetWindowActionsChecks(folder)) return 36;
    if (!runTargetCenterPinUiChecks(folder)) return 37;
    if (!runCaptureTargetUiChecks(folder)) return 38;
    if (!runInputInterruptUiChecks()) return 40;
    if (!runSessionMouseLockUiChecks()) return 41;
    if (!runForegroundSessionUiChecks()) return 44;
    if (!runSessionInputCleanupUiChecks()) return 45;
    if (!runProfileListUiChecks()) return 46;
    if (!runFeatureBatchChecks()) return 47;
    if (!runFeatureBatchUiChecks()) return 48;
    if (!runProfileMutationChecks()) return 50;
    if (!runProfileRecoveryChecks()) return 51;
    if (!runProfileActionsChecks()) return 52;
    if (!runProfileActionsUiChecks()) return 53;
    if (!runProfileStorageEdgesChecks()) return 54;
    if (!runFeatureHotkeyChecks()) return 55;
    if (!runTriggerMonitorChecks()) return 56;
    if (!runTriggerPreemptionChecks()) return 57;
    if (!runWorkflowExecutionChecks()) return 58;
    if (!runWorkflowToolbarChecks()) return 59;
    if (!runAuxiliaryToolsChecks()) return 60;
    if (!runWindowShellChecks()) return 61;
    if (!runMainWorkspaceChecks()) return 62;
    if (!runFirstTemplateRoiChecks()) return 63;
    if (!runRoiPreviewOwnershipChecks()) return 73;
    if (!runRunSessionResourcesChecks()) return 64;
    if (!runHoldTapSessionChecks()) return 65;
    if (!runWindowChromeChecks()) return 66;
    if (!runProgramSettingsChecks()) return 67;
    if (!runWindowDiagnosticsChecks()) return 68;
    if (!runWorkspaceEditorChecks()) return 69;
    if (!runPointerFeedbackChecks()) return 70;
    if (!runDocumentActionsChecks()) return 71;
    if (!runFeatureLibraryActionsChecks()) return 72;
    return 0;
}
