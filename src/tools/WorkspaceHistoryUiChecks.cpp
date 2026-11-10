#include "ui/AuxiliaryToolsController.h"
#include "app/MainWindow.h"
#include "app/Application.h"
#include "app/ProfileSwitchCoordinator.h"
#include "app/ShutdownCoordinator.h"
#include "app/WorkspaceHistoryController.h"
#include "app/WorkspaceHistoryActions.h"
#include "app/FeatureHotkeyGate.h"
#include "ui/FeatureListPanel.h"
#include "ui/memo/MemoDialog.h"
#include "ui/widgets/WorkspaceHistoryActivity.h"
#include "storage/ProfileMemoStore.h"
#include "model/Project.h"
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QMouseEvent>
#include <QTextEdit>
#include <QTimer>
#include <iostream>

namespace {
bool write(const QString& path,const QByteArray& data) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data)==data.size();
}
class InputProbe : public QWidget {
public: int presses=0;
protected: void mousePressEvent(QMouseEvent*) override { ++presses; }
};
}

class WorkspaceHistoryUiChecks {
public:
    static bool run() {
        if (QApplication::platformName()!=QStringLiteral("offscreen")) return false;
        {
            InputProbe probe;
            {
                WorkspaceHistoryActivity activity(&probe,true);
                QMouseEvent click(QEvent::MouseButtonPress,QPointF(1,1),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                QCoreApplication::sendEvent(&probe,&click);
                QEventLoop loop; QTimer::singleShot(230,&loop,&QEventLoop::quit); loop.exec();
                auto* dialog=probe.findChild<QDialog*>(QStringLiteral("workspaceHistoryActivity"));
                if (probe.presses || !FeatureHotkeyGate::isFeatureHotkeysBlocked() || !dialog || !dialog->isVisible()) return false;
                dialog->grab().save(QDir(qEnvironmentVariable("PIPBONG_DATA_DIR")).filePath(QStringLiteral("ui-history-activity.png")));
            }
            QMouseEvent click(QEvent::MouseButtonPress,QPointF(1,1),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QCoreApplication::sendEvent(&probe,&click);
            if (probe.presses!=1) return false;
        }
        MainWindow window;
        const QString originalProfile=window.m_profileManager->activeProfileId();
        const QString other=window.m_profileManager->addProfile(QStringLiteral("history guard target"));
        if (other.isEmpty() || !window.m_profileManager->setActiveProfile(originalProfile)) return false;
        const QString memoDirectory=window.m_profileManager->activeProjectDirectory();
        window.m_auxiliaryTools->ensureMemo();
        if (!window.m_auxiliaryTools->memo()->setProfile(originalProfile,memoDirectory,QStringLiteral("메모 검사"))) return false;
        auto* editor=window.m_auxiliaryTools->memo()->findChild<QTextEdit*>();
        editor->setPlainText(QStringLiteral("이전 메모"));
        if (!window.m_auxiliaryTools->memo()->saveNow()) return false;
        const QString library=QDir(Application::dataDirectory()).filePath(QStringLiteral("featureLibrary"));
        const QByteArray payload(64*1024,'u');
        for(int i=0;i<128;++i) if (!write(QDir(library).filePath(QStringLiteral("history-probe-%1.bin").arg(i)),payload)) return false;
        bool entered=false, blocked=false;
        int beats=0;
        QTimer heartbeat;
        QObject::connect(&heartbeat,&QTimer::timeout,[&] {
            ++beats;
            if (entered || !window.m_workspaceHistory->isBusy() || window.m_workspaceHistory->isPreparing()) return;
            entered=true;
            const auto count=window.m_project->features().size();
            QMetaObject::invokeMethod(window.m_featureList,"onAddFeature",Qt::DirectConnection);
            const bool nested=!window.pushGlobalUiUndoSnapshot("nested");
            const bool shutdown=!window.prepareForShutdown() && !window.m_shutdownCoordinator->checkpoint();
            const bool profile=!window.executeProfileSwitch(other,true)
                && window.m_profileManager->activeProfileId()==originalProfile;
            window.m_profileSwitchCoordinator.clearDeferredProfileSwitchId();
            window.m_modified=true;
            window.autoSaveProject(true);
            const bool saveBlocked=window.m_modified;
            editor->setPlainText(QStringLiteral("임시 메모"));
            const bool timerInvoked=QMetaObject::invokeMethod(window.m_auxiliaryTools->memo()->findChild<QTimer*>(),"timeout",Qt::DirectConnection);
            QString diskMemo;
            const bool memo=timerInvoked && ProfileMemoStore::load(memoDirectory,&diskMemo) && diskMemo==QStringLiteral("이전 메모");
            blocked=nested && shutdown && profile && saveBlocked && memo
                && count==window.m_project->features().size() && FeatureHotkeyGate::isFeatureHotkeysBlocked();
        });
        heartbeat.start(1);
        // Dirty content exercises the preparation save, rather than an already-clean shortcut.
        window.m_modified=true;
        if (!window.pushGlobalUiUndoSnapshot("history-ui-probe") || !entered || !blocked || beats==0) return false;
        heartbeat.stop();
        editor->setPlainText(QStringLiteral("변경 메모"));
        if (!window.m_auxiliaryTools->memo()->saveNow()) return false;
        window.onGlobalUndoRequested();
        QString restored;
        if (!ProfileMemoStore::load(memoDirectory,&restored) || restored!=QStringLiteral("이전 메모")
            || editor->toPlainText()!=restored) return false;
        window.onGlobalRedoRequested();
        if (!ProfileMemoStore::load(memoDirectory,&restored) || restored!=QStringLiteral("변경 메모")
            || editor->toPlainText()!=restored || window.m_workspaceHistory->isBusy()) return false;
        if (!window.prepareForShutdown()) return false;
        std::cout << "Workspace history UI: live heartbeat, consumed input/progress render, nested edits, autosave, memo timer, profile switch and shutdown guards, dirty-save preparation and memo undo/redo reload OK (offscreen; no native input)\n";
        return true;
    }
};

bool runWorkspaceHistoryUiChecks() { return WorkspaceHistoryUiChecks::run(); }
