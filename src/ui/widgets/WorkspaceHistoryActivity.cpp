#include "ui/widgets/WorkspaceHistoryActivity.h"
#include "app/FeatureHotkeyGate.h"
#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QFont>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>

namespace {
class ActivityDialog : public QDialog {
public:
    explicit ActivityDialog(QWidget* parent) : QDialog(parent) {}
    void reject() override {} // The file transaction must finish or roll back.
};
}

WorkspaceHistoryActivity::WorkspaceHistoryActivity(QWidget* parent, bool restoring)
    : m_dialog(new ActivityDialog(parent)), m_hotkeys(std::make_unique<FeatureHotkeyGateScope>()) {
    m_dialog->setObjectName(QStringLiteral("workspaceHistoryActivity"));
    m_dialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    m_dialog->setWindowModality(Qt::ApplicationModal);
    m_dialog->setMinimumWidth(360);
    m_dialog->setStyleSheet(QStringLiteral(
        "QDialog#workspaceHistoryActivity { border: 1px solid rgba(220,230,249,65); border-radius:12px;"
        " background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #34394c,stop:1 #253b43); }"
        "QProgressBar { border:0; background:rgba(15,23,35,100); border-radius:3px; min-height:6px; max-height:6px; }"
        "QProgressBar::chunk { background:#a8b6d0; border-radius:3px; }"));
    auto* layout = new QVBoxLayout(m_dialog.data());
    layout->setContentsMargins(24,22,24,22); layout->setSpacing(14);
    auto* title = new QLabel(restoring ? tr("이전 내용을 복구하고 있어요") : tr("변경 전 내용을 보관하고 있어요"));
    QFont font=title->font(); font.setPointSize(11); font.setBold(true); title->setFont(font);
    layout->addWidget(title);
    auto* detail = new QLabel(tr("완료되면 이어서 편집할 수 있어요."));
    layout->addWidget(detail);
    auto* progress = new QProgressBar; progress->setRange(0,0); progress->setTextVisible(false);
    layout->addWidget(progress);
    qApp->installEventFilter(this);
    QTimer::singleShot(180,m_dialog.data(),[dialog=m_dialog] { if(dialog)dialog->show(); });
}

WorkspaceHistoryActivity::~WorkspaceHistoryActivity() {
    if (qApp) qApp->removeEventFilter(this);
    delete m_dialog.data();
}

bool WorkspaceHistoryActivity::eventFilter(QObject*, QEvent* event) {
    switch (event->type()) {
    case QEvent::KeyPress: case QEvent::KeyRelease: case QEvent::InputMethod:
    case QEvent::MouseButtonPress: case QEvent::MouseButtonRelease: case QEvent::MouseButtonDblClick:
    case QEvent::Wheel: case QEvent::ContextMenu: case QEvent::Shortcut: case QEvent::ShortcutOverride:
    case QEvent::DragEnter: case QEvent::DragMove: case QEvent::Drop:
    case QEvent::TouchBegin: case QEvent::TouchUpdate: case QEvent::TouchEnd:
    case QEvent::Close: case QEvent::Quit:
        return true; // Consume; do not replay stale edits after restore.
    default: return false;
    }
}
