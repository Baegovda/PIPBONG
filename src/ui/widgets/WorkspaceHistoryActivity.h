#pragma once
#include <QObject>
#include <QPointer>
#include <memory>
class QWidget;
class QDialog;
class FeatureHotkeyGateScope;

/// Consumes editing input while the nested I/O wait keeps painting/timers live.
class WorkspaceHistoryActivity : public QObject {
public:
    WorkspaceHistoryActivity(QWidget* parent, bool restoring);
    ~WorkspaceHistoryActivity() override;
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    QPointer<QDialog> m_dialog;
    std::unique_ptr<FeatureHotkeyGateScope> m_hotkeys;
};
